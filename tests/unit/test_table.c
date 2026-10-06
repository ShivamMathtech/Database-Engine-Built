#include "test.h"
bool test_table(void) {
    Fixture f;
    CHECK(fixture_open(&f));
    CHECK(sql_ok(f.db, "CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT NOT NULL, score FLOAT, "
                       "active BOOLEAN)"));
    CHECK(sql_ok(f.db, "INSERT INTO t (name,id,score,active) VALUES ('Shivam',1,4,TRUE)"));
    CHECK(sql_rows(f.db, "SELECT * FROM t WHERE score=4.0 AND active", 1));
    CHECK(sql_error(f.db, "INSERT INTO t VALUES (1,'duplicate',1,FALSE)", CDB_ERR_CONSTRAINT));
    CHECK(sql_error(f.db, "INSERT INTO t VALUES (2,NULL,1,FALSE)", CDB_ERR_CONSTRAINT));
    CHECK(sql_error(f.db, "INSERT INTO t VALUES ('bad','x',1,FALSE)", CDB_ERR_TYPE));
    CHECK(sql_error(f.db, "INSERT INTO t VALUES (2,'x')", CDB_ERR_CONSTRAINT));
    CHECK(sql_error(f.db, "INSERT INTO t (id,id) VALUES (2,3)", CDB_ERR_CONSTRAINT));
    CHECK(sql_error(f.db, "CREATE TABLE t (id INTEGER)", CDB_ERR_EXISTS));
    CHECK(sql_error(f.db, "CREATE TABLE duplicate (x TEXT,x INTEGER)", CDB_ERR_CONSTRAINT));
    CHECK(sql_error(f.db, "CREATE TABLE wrongpk (x TEXT PRIMARY KEY)", CDB_ERR_UNSUPPORTED));
    CHECK(sql_error(f.db, "CREATE INDEX nope ON t(name)", CDB_ERR_UNSUPPORTED));
    CHECK(sql_error(f.db, "SELECT missing FROM t", CDB_ERR_NOT_FOUND));
    CHECK(sql_ok(f.db, "CREATE TABLE empty (id INTEGER)"));
    CHECK(sql_error(f.db, "SELECT * FROM empty WHERE missing=1", CDB_ERR_NOT_FOUND));
    CHECK(sql_error(f.db, "SELECT * FROM empty WHERE id", CDB_ERR_TYPE));
    /* Enough metadata to allocate a second catalog page. */ for (unsigned i = 0; i < 20; i++) {
        char sql[160];
        snprintf(sql, sizeof(sql),
                 "CREATE TABLE t%u (id INTEGER PRIMARY KEY, name TEXT, extra TEXT, age INTEGER)",
                 i);
        CHECK(sql_ok(f.db, sql));
    }
    CHECK(fixture_reopen(&f));
    CHECK(f.db->table_count == 22);
    CHECK(sql_rows(f.db, "SELECT * FROM t", 1));
    CHECK(sql_ok(f.db, "DROP TABLE t"));
    CHECK(sql_error(f.db, "SELECT * FROM t", CDB_ERR_NOT_FOUND));
    CHECK(fixture_reopen(&f));
    CHECK(f.db->table_count == 21);
    fixture_close(&f);
    return true;
}
