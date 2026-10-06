#include "test.h"
#include <fcntl.h>
bool fixture_open(Fixture *f) {
    memset(f, 0, sizeof(*f));
    strcpy(f->path, "/tmp/cdb-test-XXXXXX");
    int fd = mkstemp(f->path);
    CHECK(fd >= 0);
    CHECK(close(fd) == 0);
    CdbError e = {0};
    OK(cdb_open(f->path, &f->db, &e));
    f->db->log_level = -1;
    return true;
}
void fixture_close(Fixture *f) {
    cdb_close(f->db);
    f->db = NULL;
    (void)unlink(f->path);
    char wal[140];
    snprintf(wal, sizeof(wal), "%s.wal", f->path);
    (void)unlink(wal);
}
bool fixture_reopen(Fixture *f) {
    cdb_close(f->db);
    f->db = NULL;
    CdbError e = {0};
    OK(cdb_open(f->path, &f->db, &e));
    f->db->log_level = -1;
    return true;
}
bool sql_ok(Cdb *db, const char *sql) {
    CdbError e = {0};
    CdbResult r = {0};
    bool ok = cdb_execute(db, sql, &r, &e);
    if (!ok)
        fprintf(stderr, "SQL failed [%s]: %s\n", sql, e.message);
    cdb_result_free(&r);
    return ok;
}
bool sql_error(Cdb *db, const char *sql, CdbCode code) {
    CdbError e = {0};
    CdbResult r = {0};
    bool ok = cdb_execute(db, sql, &r, &e);
    cdb_result_free(&r);
    if (ok || e.code != code)
        fprintf(stderr, "Expected %s for [%s], got %s (%s)\n", cdb_code_name(code), sql,
                cdb_code_name(e.code), e.message);
    return !ok && e.code == code;
}
bool sql_rows(Cdb *db, const char *sql, size_t count) {
    CdbError e = {0};
    CdbResult r = {0};
    bool ok = cdb_execute(db, sql, &r, &e);
    bool match = ok && r.row_count == count;
    if (!match)
        fprintf(stderr, "Expected %zu rows for [%s], got %zu (%s)\n", count, sql, r.row_count,
                e.message);
    cdb_result_free(&r);
    return match;
}
uint32_t test_random(uint32_t *seed) {
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return *seed;
}
