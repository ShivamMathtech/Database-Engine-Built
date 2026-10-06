#include "cdb/row.h"
#include "test.h"
#include <math.h>
bool test_values(void) {
    CdbError e = {0};
    CdbRow r = {.count = 6};
    r.values[0] = cdb_integer(INT64_MIN);
    r.values[1] = cdb_float(3.125);
    OK(cdb_text(&r.values[2], "Shivam's database", 17, &e));
    r.values[3] = cdb_boolean(true);
    r.values[4] = cdb_null();
    r.values[5] = cdb_integer(INT64_MAX);
    uint8_t bytes[CDB_MAX_RECORD];
    size_t n;
    OK(cdb_row_serialize(&r, bytes, sizeof(bytes), &n, &e));
    CHECK(n == cdb_row_size(&r));
    CdbRow copy;
    OK(cdb_row_deserialize(&copy, bytes, n, &e));
    CHECK(copy.count == 6);
    CHECK(copy.values[0].as.integer == INT64_MIN);
    CHECK(copy.values[5].as.integer == INT64_MAX);
    CHECK(copy.values[1].as.real == 3.125);
    CHECK(copy.values[2].as.text.data != r.values[2].as.text.data);
    CHECK(strcmp(copy.values[2].as.text.data, "Shivam's database") == 0);
    cdb_row_free(&copy);
    for (size_t i = 0; i < n; i++) {
        CHECK(!cdb_row_deserialize(&copy, bytes, i, &e));
        cdb_row_free(&copy);
    }
    uint8_t saved = bytes[2];
    bytes[2] = 255;
    CHECK(!cdb_row_deserialize(&copy, bytes, n, &e));
    bytes[2] = saved;
    CdbValue a = cdb_integer(INT64_MAX), b = cdb_integer(INT64_MAX - 1);
    int cmp;
    OK(cdb_value_compare(&a, &b, &cmp, &e));
    CHECK(cmp > 0);
    b = cdb_float(9223372036854775808.0);
    OK(cdb_value_compare(&a, &b, &cmp, &e));
    CHECK(cmp < 0);
    a = cdb_integer(-9223372036854775807LL);
    b = cdb_float(-9223372036854775808.0);
    OK(cdb_value_compare(&a, &b, &cmp, &e));
    CHECK(cmp > 0);
    r.values[1] = cdb_float(INFINITY);
    CHECK(!cdb_row_serialize(&r, bytes, sizeof(bytes), &n, &e));
    cdb_row_free(&r);
    return true;
}
