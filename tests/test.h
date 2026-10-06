#ifndef CDB_TEST_H
#define CDB_TEST_H
#include "cdb/internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                           \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
#define OK(x)                                                                                      \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "FAIL %s:%d: %s: %s\n", __FILE__, __LINE__, #x, e.message);            \
            return false;                                                                          \
        }                                                                                          \
    } while (0)
typedef struct {
    char path[128];
    Cdb *db;
} Fixture;
bool fixture_open(Fixture *f);
void fixture_close(Fixture *f);
bool fixture_reopen(Fixture *f);
bool sql_ok(Cdb *db, const char *sql);
bool sql_error(Cdb *db, const char *sql, CdbCode code);
bool sql_rows(Cdb *db, const char *sql, size_t count);
uint32_t test_random(uint32_t *seed);
bool test_values(void);
bool test_lexer(void);
bool test_parser(void);
bool test_storage(void);
bool test_btree(void);
bool test_table(void);
bool test_transaction(void);
bool test_queries(void);
bool test_persistence(void);
bool test_recovery(void);
bool test_corruption(void);
#endif
