#include "test.h"
bool test_queries(void) {
    Fixture f;
    CHECK(fixture_open(&f));
    CdbError e = {0};
    CdbResult r = {0};
    CHECK(sql_ok(
        f.db,
        "CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT, age INTEGER, active BOOLEAN)"));
    CHECK(sql_ok(f.db,
                 "INSERT INTO users VALUES "
                 "(1,'Shivam',24,TRUE),(2,'Rahul',23,FALSE),(3,'Ada',24,NULL),(4,NULL,NULL,TRUE)"));
    CHECK(sql_rows(
        f.db, "SELECT id,name FROM users WHERE age>23 AND (active=TRUE OR active IS NULL)", 2));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE NULL = NULL", 0));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE NOT age=24", 1));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age IS NULL", 1));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age IS NOT NULL", 3));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE TRUE OR NULL", 4));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE FALSE AND NULL", 0));
    CHECK(sql_rows(f.db, "SELECT * FROM users LIMIT 2", 2));
    CHECK(sql_rows(f.db, "SELECT * FROM users LIMIT 0", 0));
    OK(cdb_execute(f.db, "SELECT * FROM users WHERE age=24", &r, &e));
    CHECK(r.examined == 4 && strstr(r.plan, "FULL TABLE SCAN"));
    cdb_result_free(&r);
    CHECK(sql_ok(f.db, "CREATE INDEX age_ix ON users(age)"));
    OK(cdb_execute(f.db, "SELECT * FROM users WHERE age=24", &r, &e));
    CHECK(r.row_count == 2 && r.examined == 2 && strstr(r.plan, "INDEX LOOKUP"));
    cdb_result_free(&r);
    OK(cdb_execute(f.db, "EXPLAIN SELECT * FROM users WHERE id=1", &r, &e));
    CHECK(r.row_count == 1 && strstr(r.rows[0].values[0].as.text.data, "PRIMARY KEY"));
    cdb_result_free(&r);
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE 24=age AND id=1", 1));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=24 OR id=2", 3));
    CHECK(sql_ok(f.db, "UPDATE users SET age=age+2, name='updated' WHERE age=24"));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=24", 0));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=26", 2));
    CHECK(sql_ok(f.db, "DELETE FROM users WHERE age=26"));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=26", 0));
    CHECK(sql_rows(f.db, "SELECT * FROM users", 2));
    CHECK(sql_ok(f.db, "UPDATE users SET id=10 WHERE id=2"));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE id=2", 0));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE id=10", 1));
    CHECK(sql_error(f.db, "UPDATE users SET age=1/0", CDB_ERR_TYPE));
    CHECK(sql_error(f.db, "UPDATE users SET age=9223372036854775807+1", CDB_ERR_TYPE));
    CHECK(sql_error(f.db, "UPDATE users SET age=-9223372036854775808/-1", CDB_ERR_TYPE));
    CHECK(sql_error(f.db, "UPDATE users SET age=9223372036854775807*2", CDB_ERR_TYPE));
    CHECK(sql_ok(f.db, "UPDATE users SET age=(8-2)*3/2 WHERE id=10"));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=9", 1));
    CHECK(fixture_reopen(&f));
    CHECK(sql_rows(f.db, "SELECT * FROM users WHERE age=9", 1));
    fixture_close(&f);
    return true;
}
