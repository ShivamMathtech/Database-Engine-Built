#include "test.h"
int main(void) {
    struct {
        const char *name;
        bool (*run)(void);
    } tests[] = {{"values / row codec", test_values},
                 {"lexer", test_lexer},
                 {"parser / malformed SQL", test_parser},
                 {"pages / pager / buffer pool", test_storage},
                 {"B-Tree split / merge / deletion", test_btree},
                 {"tables / constraints / catalog", test_table},
                 {"transactions / rollback", test_transaction},
                 {"queries / planner / index maintenance", test_queries},
                 {"persistence / relocation", test_persistence},
                 {"crash recovery", test_recovery},
                 {"corrupt files / WAL", test_corruption}};
    size_t count = sizeof(tests) / sizeof(tests[0]);
    size_t skipped = 0;
    bool skip_crashes = getenv("CDB_SKIP_CRASH_TESTS") != NULL;
    for (size_t i = 0; i < count; i++) {
        if (skip_crashes && (tests[i].run == test_recovery || tests[i].run == test_corruption)) {
            printf("[%zu/%zu] %s ... SKIP (intentional crash children)\n", i + 1, count,
                   tests[i].name);
            skipped++;
            continue;
        }
        printf("[%zu/%zu] %s ... ", i + 1, count, tests[i].name);
        fflush(stdout);
        if (!tests[i].run()) {
            puts("FAIL");
            return 1;
        }
        puts("PASS");
    }
    printf("%zu test groups passed; %zu skipped.\n", count - skipped, skipped);
    return 0;
}
