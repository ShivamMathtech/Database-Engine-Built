#include "cdb/parser.h"
#include "test.h"
bool test_parser(void) {
    CdbError e = {0};
    CdbQuery q;
    OK(cdb_parse(
        "SELECT id, name FROM users WHERE age > 18 AND (active = TRUE OR active IS NULL) LIMIT 9;",
        &q, &e));
    CHECK(q.kind == Q_SELECT && q.name_count == 2 && q.limit == 9 && q.where->op == TK_AND);
    cdb_query_free(&q);
    OK(cdb_parse("INSERT INTO x VALUES (-9223372036854775808, 'it''s'), (2, NULL)", &q, &e));
    CHECK(q.row_count == 2 && q.rows[0].values[0].as.integer == INT64_MIN &&
          strcmp(q.rows[0].values[1].as.text.data, "it's") == 0);
    cdb_query_free(&q);
    const char *valid[] = {"CREATE TABLE x (id INTEGER PRIMARY KEY, v TEXT NOT NULL)",
                           "CREATE INDEX age_ix ON users(age)",
                           "UPDATE x SET id=id+1, v=NULL WHERE NOT id=2",
                           "DELETE FROM x WHERE id IS NOT NULL",
                           "DROP TABLE x",
                           "BEGIN",
                           "COMMIT",
                           "ROLLBACK",
                           "EXPLAIN SELECT * FROM x"};
    for (size_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        OK(cdb_parse(valid[i], &q, &e));
        cdb_query_free(&q);
    }
    const char *bad[] = {"",
                         "SELECT FROM x",
                         "SELECT * FROM x; DELETE FROM x",
                         "CREATE TABLE x ()",
                         "INSERT INTO x VALUES (1,)",
                         "INSERT INTO x VALUES (9223372036854775808)",
                         "SELECT * FROM x WHERE x =",
                         "SELECT * FROM x LIMIT -1",
                         "CREATE TABLE x (a INTEGER,)",
                         "EXPLAIN DELETE FROM x"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        CHECK(!cdb_parse(bad[i], &q, &e));
        cdb_query_free(&q);
    }
    char deep[400];
    strcpy(deep, "SELECT * FROM x WHERE ");
    size_t at = strlen(deep);
    for (size_t i = 0; i < 100; i++)
        deep[at++] = '(';
    deep[at++] = '1';
    for (size_t i = 0; i < 100; i++)
        deep[at++] = ')';
    deep[at] = '\0';
    CHECK(!cdb_parse(deep, &q, &e));
    CHECK(e.code == CDB_ERR_LIMIT);
    uint32_t seed = 42;
    char fuzz[129];
    for (size_t i = 0; i < 10000; i++) {
        size_t n = test_random(&seed) % 128u;
        for (size_t j = 0; j < n; j++)
            fuzz[j] = (char)(32u + test_random(&seed) % 95u);
        fuzz[n] = '\0';
        (void)cdb_parse(fuzz, &q, &e);
        cdb_query_free(&q);
    }
    return true;
}
