#include "cdb/wal.h"
#include "cdb/internal.h"
#include "cdb/page.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WAL_HEADER 64u
#define WAL_RECORD (4u + CDB_PAGE_SIZE)
#define WAL_FOOTER 16u
static void crash_point(const char *point) {
#ifdef CDB_TEST_FAULTS
    const char *requested = getenv("CDB_CRASH_POINT");
    if (requested && strcmp(requested, point) == 0)
        _exit(86);
#else
    (void)point;
#endif
}
static bool clear_wal(Cdb *db, CdbError *e) {
    if (ftruncate(db->pager.wal_fd, 0) < 0)
        return cdb_fail(e, CDB_ERR_IO, "Cannot truncate WAL: %s", strerror(errno));
    return cdb_io_sync(db->pager.wal_fd, e);
}
bool cdb_wal_commit(Cdb *db, CdbError *e) {
    uint8_t header[WAL_HEADER] = {0};
    memcpy(header, "CDBW", 4);
    cdb_put32(header + 4, 1);
    cdb_put32(header + 8, CDB_PAGE_SIZE);
    cdb_put32(header + 12, (uint32_t)db->tx.dirty_count);
    cdb_put32(header + 16, db->page_count);
    cdb_put64(header + 24, db->generation + 1);
    memcpy(header + 32, db->identity, 16);
    cdb_put32(header + 60, cdb_hash(UINT32_C(2166136261), header, 60));
    if (ftruncate(db->pager.wal_fd, 0) < 0)
        return cdb_fail(e, CDB_ERR_IO, "Cannot reset WAL");
    CDB_TRY(cdb_io_write(db->pager.wal_fd, header, sizeof(header), 0, e));
    uint32_t checksum = cdb_hash(UINT32_C(2166136261), header, sizeof(header));
    uint64_t offset = WAL_HEADER;
    crash_point("wal_header");
    for (size_t i = 0; i < db->tx.map_size; i++) {
        CdbChange *change = db->tx.map[i];
        if (!change)
            continue;
        cdb_seal(change->data);
        uint8_t id[4];
        cdb_put32(id, change->id);
        CDB_TRY(cdb_io_write(db->pager.wal_fd, id, 4, offset, e));
        offset += 4;
        CDB_TRY(cdb_io_write(db->pager.wal_fd, change->data, CDB_PAGE_SIZE, offset, e));
        offset += CDB_PAGE_SIZE;
        checksum = cdb_hash(checksum, id, 4);
        checksum = cdb_hash(checksum, change->data, CDB_PAGE_SIZE);
    }
    crash_point("wal_records");
    uint8_t footer[WAL_FOOTER];
    memcpy(footer, "CMT1", 4);
    cdb_put64(footer + 4, db->generation + 1);
    cdb_put32(footer + 12, checksum);
    CDB_TRY(cdb_io_write(db->pager.wal_fd, footer, sizeof(footer), offset, e));
    CDB_TRY(cdb_io_sync(db->pager.wal_fd, e));
    crash_point("wal_sync");
    size_t applied = 0;
    for (size_t i = 0; i < db->tx.map_size; i++) {
        CdbChange *change = db->tx.map[i];
        if (!change)
            continue;
        CDB_TRY(cdb_pager_write(&db->pager, change->id, change->data, e));
        if (++applied == 1)
            crash_point("mid_apply");
    }
    CDB_TRY(cdb_io_sync(db->pager.fd, e));
    crash_point("db_sync");
    return clear_wal(db, e);
}
bool cdb_wal_recover(Cdb *db, CdbError *e) {
    uint64_t bytes;
    CDB_TRY(cdb_io_size(db->pager.wal_fd, &bytes, e));
    if (!bytes)
        return true;
    if (bytes < WAL_HEADER)
        return clear_wal(db, e);
    uint8_t header[WAL_HEADER];
    CDB_TRY(cdb_io_read(db->pager.wal_fd, header, sizeof(header), 0, e));
    if (memcmp(header, "CDBW", 4) || cdb_get32(header + 4) != 1 ||
        cdb_get32(header + 8) != CDB_PAGE_SIZE ||
        cdb_get32(header + 60) != cdb_hash(UINT32_C(2166136261), header, 60))
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid WAL header; preserve files for diagnosis");
    uint32_t count = cdb_get32(header + 12), pages = cdb_get32(header + 16);
    uint64_t generation = cdb_get64(header + 24);
    if (!count || count > CDB_MAX_TX_PAGES || pages < 2 || pages > CDB_MAX_PAGES || count > pages ||
        !generation)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid WAL page counts/generation");
    uint64_t expected = WAL_HEADER + (uint64_t)count * WAL_RECORD + WAL_FOOTER;
    /* Incomplete transactions never reached page application, so their prefix is discardable. */
    if (bytes < expected)
        return clear_wal(db, e);
    if (bytes != expected)
        return cdb_fail(e, CDB_ERR_CORRUPT, "WAL has unexpected trailing bytes");
    uint8_t footer[WAL_FOOTER];
    CDB_TRY(cdb_io_read(db->pager.wal_fd, footer, sizeof(footer), expected - WAL_FOOTER, e));
    if (memcmp(footer, "CMT1", 4) || cdb_get64(footer + 4) != generation)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid WAL commit marker");
    uint8_t *seen = calloc(pages, 1);
    if (!seen)
        return cdb_fail(e, CDB_ERR_MEMORY, "Cannot validate WAL page ids");
    uint32_t checksum = cdb_hash(UINT32_C(2166136261), header, sizeof(header));
    uint8_t record[WAL_RECORD];
    bool ok = true;
    uint64_t offset = WAL_HEADER;
    for (uint32_t i = 0; i < count && ok; i++, offset += WAL_RECORD) {
        if (!cdb_io_read(db->pager.wal_fd, record, sizeof(record), offset, e)) {
            ok = false;
            break;
        }
        uint32_t id = cdb_get32(record);
        uint8_t *p = record + 4;
        if (id >= pages || seen[id] || !cdb_checksum(p)) {
            ok = cdb_fail(e, CDB_ERR_CORRUPT, "Invalid/checksum-damaged WAL page");
            break;
        }
        seen[id] = 1;
        if (id == 0) {
            if (memcmp(p, "CDB1", 4) || cdb_get32(p + 4) != 1 ||
                cdb_get32(p + 8) != CDB_PAGE_SIZE || cdb_get32(p + 12) != pages ||
                cdb_get64(p + 16) != generation || memcmp(p + 32, header + 32, 16))
                ok = cdb_fail(e, CDB_ERR_CORRUPT, "WAL superblock mismatch");
        } else
            ok = cdb_page_validate(p, id, e);
        checksum = cdb_hash(checksum, record, sizeof(record));
    }
    if (ok && (!seen[0] || checksum != cdb_get32(footer + 12)))
        ok = cdb_fail(e, CDB_ERR_CORRUPT, "WAL transaction checksum/superblock mismatch");
    free(seen);
    if (!ok)
        return false;
    /* A valid old/new superblock must belong to this WAL. Torn superblocks are repaired by redo. */
    uint64_t db_bytes;
    CDB_TRY(cdb_io_size(db->pager.fd, &db_bytes, e));
    if (db_bytes >= CDB_PAGE_SIZE) {
        uint8_t old[CDB_PAGE_SIZE];
        CDB_TRY(cdb_io_read(db->pager.fd, old, sizeof(old), 0, e));
        if (cdb_checksum(old) && memcmp(old, "CDB1", 4) == 0) {
            uint64_t gen = cdb_get64(old + 16);
            if (memcmp(old + 32, header + 32, 16) ||
                !(generation == gen || (gen != UINT64_MAX && generation == gen + 1)))
                return cdb_fail(e, CDB_ERR_CORRUPT,
                                "WAL belongs to a different database/generation");
        }
    }
    /* Verification above is a separate pass: never partially apply a corrupt log. */ offset =
        WAL_HEADER;
    for (uint32_t i = 0; i < count; i++, offset += WAL_RECORD) {
        CDB_TRY(cdb_io_read(db->pager.wal_fd, record, sizeof(record), offset, e));
        CDB_TRY(cdb_pager_write(&db->pager, cdb_get32(record), record + 4, e));
    }
    CDB_TRY(cdb_io_sync(db->pager.fd, e));
    CDB_TRY(clear_wal(db, e));
    cdb_log(db, 2, "recovered transaction %llu (%u pages)", (unsigned long long)generation, count);
    return true;
}
