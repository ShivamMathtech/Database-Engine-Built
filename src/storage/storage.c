#include "cdb/internal.h"
#include "cdb/page.h"
#include <stdlib.h>
#include <string.h>
static bool ensure_map(Cdb *db, uint32_t id, CdbError *e) {
    if ((size_t)id < db->tx.map_size)
        return true;
    size_t n = db->tx.map_size ? db->tx.map_size : 16;
    while (n <= (size_t)id)
        n *= 2;
    CdbChange **map = realloc(db->tx.map, n * sizeof(*map));
    if (!map)
        return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate transaction page map");
    memset(map + db->tx.map_size, 0, (n - db->tx.map_size) * sizeof(*map));
    db->tx.map = map;
    db->tx.map_size = n;
    return true;
}
bool cdb_storage_read(Cdb *db, uint32_t id, uint8_t *out, CdbError *e) {
    if (id >= db->page_count)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Page %u is outside database (%u pages)", id,
                        db->page_count);
    if (db->tx.active && id < db->tx.map_size && db->tx.map[id])
        memcpy(out, db->tx.map[id]->data, CDB_PAGE_SIZE);
    else {
        CdbFrame *frame;
        CDB_TRY(cdb_buffer_pin(&db->cache, id, &frame, e));
        memcpy(out, frame->data, CDB_PAGE_SIZE);
        cdb_buffer_unpin(frame);
    }
    return id == 0 || cdb_page_validate(out, id, e);
}
bool cdb_storage_edit(Cdb *db, uint32_t id, uint8_t **out, CdbError *e) {
    if (!db->tx.active)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "Page write requires a transaction");
    if (id >= db->page_count)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid page to edit");
    CDB_TRY(ensure_map(db, id, e));
    if (!db->tx.map[id]) {
        if (db->tx.dirty_count == CDB_MAX_TX_PAGES)
            return cdb_fail(e, CDB_ERR_LIMIT, "Transaction exceeds %u dirty pages",
                            CDB_MAX_TX_PAGES);
        CdbChange *change = malloc(sizeof(*change));
        if (!change)
            return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate dirty page");
        change->id = id;
        if (!cdb_storage_read(db, id, change->data, e)) {
            free(change);
            return false;
        }
        db->tx.map[id] = change;
        db->tx.dirty_count++;
    }
    *out = db->tx.map[id]->data;
    return true;
}
bool cdb_storage_allocate(Cdb *db, uint8_t type, uint32_t owner, uint32_t *id, CdbError *e) {
    if (!db->tx.active)
        return cdb_fail(e, CDB_ERR_TRANSACTION, "Allocation requires a transaction");
    if (db->page_count >= CDB_MAX_PAGES || db->tx.dirty_count >= CDB_MAX_TX_PAGES)
        return cdb_fail(e, CDB_ERR_LIMIT, "Page or transaction limit reached");
    uint32_t next = db->page_count;
    CDB_TRY(ensure_map(db, next, e));
    CdbChange *change = calloc(1, sizeof(*change));
    if (!change)
        return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate page");
    change->id = next;
    if (next)
        cdb_page_init(change->data, type, next, owner);
    db->tx.map[next] = change;
    db->tx.dirty_count++;
    db->page_count++;
    *id = next;
    cdb_log(db, 4, "allocate page %u type %u", next, (unsigned)type);
    return true;
}
bool cdb_storage_header(Cdb *db, CdbError *e) {
    uint8_t *p;
    CDB_TRY(cdb_storage_edit(db, 0, &p, e));
    memset(p, 0, CDB_PAGE_SIZE);
    memcpy(p, "CDB1", 4);
    cdb_put32(p + 4, 1);
    cdb_put32(p + 8, CDB_PAGE_SIZE);
    cdb_put32(p + 12, db->page_count);
    cdb_put64(p + 16, db->generation + 1);
    cdb_put32(p + 24, 1);
    memcpy(p + 32, db->identity, 16);
    return true;
}
