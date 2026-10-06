#include "cdb/internal.h"
#include "cdb/wal.h"
#include <stdlib.h>
#include <string.h>
bool cdb_transaction_begin(Cdb *db, CdbError *e) {
    if (db->poisoned)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "Connection requires close/reopen for recovery");
    if (db->tx.active)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "Nested transactions are unsupported");
    memset(&db->tx, 0, sizeof(db->tx));
    db->tx.active = true;
    db->tx.original_pages = db->page_count;
    cdb_log(db, 3, "BEGIN generation %llu", (unsigned long long)db->generation);
    return true;
}
void cdb_transaction_discard(Cdb *db) {
    for (size_t i = 0; i < db->tx.map_size; i++)
        free(db->tx.map[i]);
    free(db->tx.map);
    memset(&db->tx, 0, sizeof(db->tx));
}
bool cdb_transaction_commit(Cdb *db, CdbError *e) {
    if (!db->tx.active)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "No active transaction");
    if (db->poisoned)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "Connection requires recovery");
    if (!db->tx.dirty_count) {
        cdb_transaction_discard(db);
        return true;
    }
    if (db->generation == UINT64_MAX)
        return cdb_fail(e, CDB_ERR_LIMIT, "Transaction counter exhausted");
    CDB_TRY(cdb_storage_header(db, e));
    /* Any I/O failure can make the commit outcome uncertain. Never roll it back in memory and
     * continue. */
    if (!cdb_wal_commit(db, e)) {
        db->poisoned = true;
        return false;
    }
    db->generation++;
    cdb_transaction_discard(db);
    cdb_buffer_invalidate(&db->cache);
    cdb_log(db, 3, "COMMIT generation %llu", (unsigned long long)db->generation);
    return true;
}
bool cdb_transaction_rollback(Cdb *db, CdbError *e) {
    if (!db->tx.active)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "No active transaction");
    if (db->poisoned)
        return cdb_fail(e, CDB_ERR_TRANSACTION,
                        "Commit outcome uncertain; close/reopen instead of ROLLBACK");
    db->page_count = db->tx.original_pages;
    cdb_transaction_discard(db);
    if (!cdb_catalog_load(db, e) || !cdb_index_rebuild(db, e)) {
        db->poisoned = true;
        return false;
    }
    cdb_log(db, 3, "ROLLBACK");
    return true;
}
