#include "cdb/executor.h"
#include "cdb/internal.h"
#include "cdb/page.h"
#include "cdb/parser.h"
#include "cdb/wal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
void cdb_log(Cdb *db, int level, const char *fmt, ...) {
    if (level > db->log_level)
        return;
    static const char *const names[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    fprintf(stderr, "[CDB %s] ", names[level]);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}
void cdb_result_free(CdbResult *r) {
    if (!r)
        return;
    for (size_t i = 0; i < r->row_count; i++)
        cdb_row_free(&r->rows[i]);
    free(r->rows);
    memset(r, 0, sizeof(*r));
}
void cdb_close(Cdb *db) {
    if (!db)
        return;
    cdb_transaction_discard(db);
    cdb_catalog_free(db);
    cdb_pager_close(&db->pager);
    free(db);
}
bool cdb_open(const char *path, Cdb **out, CdbError *e) {
    cdb_error_clear(e);
    if (!out)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Missing output pointer");
    *out = NULL;
    if (!path || !*path)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Database path is empty");
    Cdb *db = calloc(1, sizeof(*db));
    if (!db)
        return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate database connection");
    db->pager.fd = -1;
    db->pager.wal_fd = -1;
    db->log_level = 0;
    const char *level = getenv("CDB_LOG_LEVEL");
    const char *const levels[] = {"ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
    for (int i = 0; i < 5; i++)
        if (level && strcmp(level, levels[i]) == 0)
            db->log_level = i;
    if (!cdb_pager_open(&db->pager, path, e))
        goto fail;
    cdb_buffer_init(&db->cache, &db->pager);
    if (!cdb_wal_recover(db, e))
        goto fail;
    uint64_t size;
    if (!cdb_io_size(db->pager.fd, &size, e))
        goto fail;
    if (size == 0) {
        int random_fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
        if (random_fd < 0) {
            cdb_fail(e, CDB_ERR_IO, "Cannot read database identity from /dev/urandom");
            goto fail;
        }
        bool random_ok = true;
        size_t have = 0;
        while (have < sizeof(db->identity)) {
            ssize_t got = read(random_fd, db->identity + have, sizeof(db->identity) - have);
            if (got < 0 && errno == EINTR)
                continue;
            if (got <= 0) {
                random_ok = cdb_fail(e, CDB_ERR_IO, "Cannot read database identity");
                break;
            }
            have += (size_t)got;
        }
        (void)close(random_fd);
        if (!random_ok)
            goto fail;
        uint32_t id;
        if (!cdb_transaction_begin(db, e) || !cdb_storage_allocate(db, 0, 0, &id, e) ||
            !cdb_storage_allocate(db, CDB_PAGE_CATALOG, 0, &id, e) ||
            !cdb_transaction_commit(db, e))
            goto fail;
    } else {
        if (size < 2u * CDB_PAGE_SIZE || size % CDB_PAGE_SIZE ||
            size / CDB_PAGE_SIZE > CDB_MAX_PAGES) {
            cdb_fail(e, CDB_ERR_CORRUPT, "Invalid database file length");
            goto fail;
        }
        uint8_t p[CDB_PAGE_SIZE];
        if (!cdb_pager_read(&db->pager, 0, p, e))
            goto fail;
        if (memcmp(p, "CDB1", 4) || cdb_get32(p + 4) != 1 || cdb_get32(p + 8) != CDB_PAGE_SIZE ||
            cdb_get32(p + 24) != 1 || cdb_get32(p + 12) != size / CDB_PAGE_SIZE ||
            !cdb_get64(p + 16)) {
            cdb_fail(e, CDB_ERR_CORRUPT, "Unsupported or corrupt CDB superblock");
            goto fail;
        }
        db->page_count = cdb_get32(p + 12);
        db->generation = cdb_get64(p + 16);
        memcpy(db->identity, p + 32, 16);
    }
    if (!cdb_catalog_load(db, e) || !cdb_index_rebuild(db, e))
        goto fail;
    cdb_log(db, 2, "opened %s (%u pages, %zu tables)", db->pager.path, db->page_count,
            db->table_count);
    *out = db;
    return true;
fail:
    cdb_close(db);
    return false;
}
bool cdb_execute(Cdb *db, const char *sql, CdbResult *r, CdbError *e) {
    cdb_error_clear(e);
    if (!r)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Missing result pointer");
    memset(r, 0, sizeof(*r));
    if (!db)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Missing database connection");
    if (db->poisoned)
        return cdb_fail(e, CDB_ERR_TRANSACTION,
                        "Connection requires close/reopen for WAL recovery");
    CdbQuery q;
    if (!cdb_parse(sql, &q, e))
        return false;
    bool write = q.kind == Q_CREATE || q.kind == Q_DROP || q.kind == Q_INSERT ||
                 q.kind == Q_UPDATE || q.kind == Q_DELETE || q.kind == Q_INDEX;
    bool automatic = write && !db->tx.active;
    bool ok = true;
    if (automatic)
        ok = cdb_transaction_begin(db, e);
    if (ok)
        ok = cdb_executor_run(db, &q, r, e);
    if (ok && automatic)
        ok = cdb_transaction_commit(db, e);
    if (!ok && write && db->tx.active && !db->poisoned) {
        CdbError original = {0}, rollback = {0};
        if (e)
            original = *e;
        bool restored = cdb_transaction_rollback(db, &rollback);
        if (e) {
            *e = original;
            size_t n = strlen(e->message);
            snprintf(e->message + n, sizeof(e->message) - n, "; %s",
                     restored ? "transaction rolled back" : "rollback failed; reopen required");
        }
    }
    if (ok)
        snprintf(r->message, sizeof(r->message), "OK (%llu rows affected)",
                 (unsigned long long)r->affected);
    else {
        cdb_log(db, 0, "%s", e ? e->message : "Query failed");
        cdb_result_free(r);
    }
    cdb_query_free(&q);
    return ok;
}
bool cdb_meta(Cdb *db, const char *command, FILE *out, CdbError *e) {
    cdb_error_clear(e);
    if (strcmp(command, ".help") == 0) {
        fputs("SQL ends with ';'. Meta commands occupy their own line.\n"
              ".help  .tables  .schema [table]  .stats  .version  .check\n"
              ".btree table column  .quit  .exit\n"
              "SQL: CREATE/DROP TABLE, INSERT, SELECT, UPDATE, DELETE, CREATE INDEX,\n"
              "BEGIN, COMMIT, ROLLBACK, EXPLAIN SELECT.\n",
              out);
        return true;
    }
    if (strcmp(command, ".version") == 0) {
        fprintf(out, "CDB v%s (file format 1)\n", CDB_VERSION);
        return true;
    }
    if (strcmp(command, ".tables") == 0) {
        for (size_t i = 0; i < db->table_count; i++)
            fprintf(out, "%s\n", db->tables[i].name);
        return true;
    }
    if (strcmp(command, ".stats") == 0) {
        fprintf(
            out,
            "pages=%u tables=%zu generation=%llu transaction=%s dirty_pages=%zu\n"
            "page_reads=%llu page_writes=%llu cache_hits=%llu cache_misses=%llu evictions=%llu\n",
            db->page_count, db->table_count, (unsigned long long)db->generation,
            db->tx.active ? "active" : "none", db->tx.dirty_count,
            (unsigned long long)db->pager.reads, (unsigned long long)db->pager.writes,
            (unsigned long long)db->cache.hits, (unsigned long long)db->cache.misses,
            (unsigned long long)db->cache.evictions);
        return true;
    }
    if (strcmp(command, ".schema") == 0 || strncmp(command, ".schema ", 8) == 0) {
        const char *filter = strlen(command) > 7 ? command + 8 : NULL;
        bool found = false;
        for (size_t i = 0; i < db->table_count; i++) {
            CdbTable *t = &db->tables[i];
            if (filter && strcmp(filter, t->name) != 0)
                continue;
            found = true;
            fprintf(out, "CREATE TABLE %s (\n", t->name);
            for (uint16_t j = 0; j < t->column_count; j++) {
                CdbColumn *c = &t->columns[j];
                fprintf(out, "  %s %s%s%s%s\n", c->name, cdb_type_name(c->type),
                        c->primary ? " PRIMARY KEY" : "",
                        c->not_null && !c->primary ? " NOT NULL" : "",
                        j + 1 < t->column_count ? "," : "");
            }
            fputs(");\n", out);
            for (uint16_t j = 0; j < t->column_count; j++)
                if (t->columns[j].index_name[0])
                    fprintf(out, "CREATE INDEX %s ON %s(%s);\n", t->columns[j].index_name, t->name,
                            t->columns[j].name);
        }
        return !filter || found || cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown table '%s'", filter);
    }
    if (strcmp(command, ".check") == 0) {
        for (uint32_t id = 1; id < db->page_count; id++) {
            uint8_t page[CDB_PAGE_SIZE];
            CDB_TRY(cdb_storage_read(db, id, page, e));
        }
        for (size_t i = 0; i < db->table_count; i++)
            for (uint16_t j = 0; j < db->tables[i].column_count; j++)
                CDB_TRY(cdb_btree_validate(&db->tables[i].indexes[j], e));
        fputs("Page structure/checksums and B-Tree structure: OK\n", out);
        return true;
    }
    if (strncmp(command, ".btree ", 7) == 0) {
        char table[32], column[32], extra;
        if (sscanf(command + 7, "%31s %31s %c", table, column, &extra) != 2)
            return cdb_fail(e, CDB_ERR_SYNTAX, "Usage: .btree table column");
        CdbTable *t = cdb_catalog_find(db, table);
        int col = t ? cdb_table_column(t, column) : -1;
        if (col < 0 || !(t->columns[col].primary || t->columns[col].index_name[0]))
            return cdb_fail(e, CDB_ERR_NOT_FOUND, "Indexed column not found");
        cdb_btree_print(&t->indexes[col], out);
        return true;
    }
    return cdb_fail(e, CDB_ERR_SYNTAX, "Unknown meta command; use .help");
}
