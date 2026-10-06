#include "test.h"
bool test_persistence(void) {
    Fixture f;
    CHECK(fixture_open(&f));
    CHECK(sql_ok(f.db, "CREATE TABLE t (id INTEGER PRIMARY KEY, body TEXT, group_id INTEGER)"));
    CHECK(sql_ok(f.db, "BEGIN"));
    char sql[4300];
    for (unsigned i = 0; i < 1000; i++) {
        snprintf(sql, sizeof(sql), "INSERT INTO t VALUES (%u,'small',%u)", i, i % 10u);
        CHECK(sql_ok(f.db, sql));
    }
    CHECK(sql_ok(f.db, "COMMIT"));
    CHECK(sql_ok(f.db, "CREATE INDEX group_ix ON t(group_id)"));
    CHECK(fixture_reopen(&f));
    CHECK(sql_rows(f.db, "SELECT * FROM t", 1000));
    char big[3501];
    memset(big, 'z', 3500);
    big[3500] = '\0';
    snprintf(sql, sizeof(sql), "UPDATE t SET body='%s', group_id=42 WHERE id=0", big);
    CHECK(sql_ok(f.db, sql));
    CHECK(sql_rows(f.db, "SELECT * FROM t WHERE group_id=42", 1));
    CHECK(sql_rows(f.db, "SELECT * FROM t WHERE group_id=0", 99));
    CHECK(fixture_reopen(&f));
    CdbError e = {0};
    CdbResult r = {0};
    OK(cdb_execute(f.db, "SELECT body FROM t WHERE id=0", &r, &e));
    CHECK(r.row_count == 1 && r.rows[0].values[0].as.text.length == 3500);
    cdb_result_free(&r);
    Cdb *second = NULL;
    CHECK(!cdb_open(f.path, &second, &e));
    CHECK(e.code == CDB_ERR_BUSY);
    cdb_close(second);
    CHECK(sql_ok(f.db, "DELETE FROM t WHERE id<500"));
    CHECK(fixture_reopen(&f));
    CHECK(sql_rows(f.db, "SELECT * FROM t", 500));
    fixture_close(&f);
    return true;
}
