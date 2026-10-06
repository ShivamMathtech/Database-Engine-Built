#include "test.h"
#include <errno.h>
static bool run(size_t count) {
    Fixture f;
    CHECK(fixture_open(&f));
    CHECK(sql_ok(f.db,
                 "CREATE TABLE dataset (id INTEGER PRIMARY KEY, group_id INTEGER, payload TEXT)"));
    CHECK(sql_ok(f.db, "CREATE INDEX groups ON dataset(group_id)"));
    char sql[256];
    CHECK(sql_ok(f.db, "BEGIN"));
    for (size_t i = 0; i < count; i++) {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO dataset VALUES (%zu,%zu,'deterministic payload %zu')", i, i % 100u,
                 i);
        CHECK(sql_ok(f.db, sql));
        if ((i + 1) % 5000u == 0) {
            CHECK(sql_ok(f.db, "COMMIT"));
            if (i + 1 < count)
                CHECK(sql_ok(f.db, "BEGIN"));
        }
    }
    if (f.db->tx.active)
        CHECK(sql_ok(f.db, "COMMIT"));
    CHECK(fixture_reopen(&f));
    CdbError e = {0};
    CdbTable *t = cdb_catalog_find(f.db, "dataset");
    CHECK(t != NULL);
    CHECK(t->indexes[0].count == count);
    OK(cdb_btree_validate(&t->indexes[0], &e));
    OK(cdb_btree_validate(&t->indexes[1], &e));
    uint32_t seed = 42;
    for (unsigned i = 0; i < 2000; i++) {
        size_t id = test_random(&seed) % count;
        snprintf(sql, sizeof(sql), "SELECT * FROM dataset WHERE id=%zu", id);
        CHECK(sql_rows(f.db, sql, 1));
    }
    for (size_t group = 0; group < 100; group++) {
        snprintf(sql, sizeof(sql), "SELECT * FROM dataset WHERE group_id=%zu", group);
        size_t expected = count / 100u + (group < count % 100u ? 1u : 0u);
        CHECK(sql_rows(f.db, sql, expected));
    }
    CHECK(sql_ok(f.db, "BEGIN"));
    CHECK(sql_ok(f.db, "UPDATE dataset SET group_id=101 WHERE id<1000"));
    CHECK(sql_ok(f.db, "ROLLBACK"));
    CHECK(sql_rows(f.db, "SELECT * FROM dataset WHERE group_id=101", 0));
    CHECK(sql_ok(f.db, "DELETE FROM dataset WHERE id<1000"));
    CHECK(fixture_reopen(&f));
    t = cdb_catalog_find(f.db, "dataset");
    CHECK(t->indexes[0].count == count - 1000u);
    OK(cdb_btree_validate(&t->indexes[0], &e));
    printf("Stress PASS: %zu inserted/reopened, 2000 primary-key queries, 100 duplicate-key "
           "queries, rollback and 1000 deletes; %u pages\n",
           count, f.db->page_count);
    fixture_close(&f);
    return true;
}
int main(int argc, char **argv) {
    size_t count = 100000;
    if (argc > 2) {
        fputs("Usage: stress [1000..1000000]\n", stderr);
        return 2;
    }
    if (argc == 2) {
        errno = 0;
        char *end;
        unsigned long n = strtoul(argv[1], &end, 10);
        if (errno || *end || n < 1000 || n > 1000000)
            return 2;
        count = (size_t)n;
    }
    return run(count) ? 0 : 1;
}
