#ifndef CDB_VALUE_H
#define CDB_VALUE_H
#include "error.h"
#include "types.h"
/* TEXT owns a NUL-terminated allocation. Copy with cdb_value_copy, never shallow-copy owners. */
typedef struct {
    CdbType type;
    union {
        int64_t integer;
        double real;
        bool boolean;
        struct {
            char *data;
            size_t length;
        } text;
    } as;
} CdbValue;
CdbValue cdb_null(void);
CdbValue cdb_integer(int64_t n);
CdbValue cdb_float(double n);
CdbValue cdb_boolean(bool b);
bool cdb_text(CdbValue *v, const char *s, size_t n, CdbError *e);
bool cdb_value_copy(CdbValue *dst, const CdbValue *src, CdbError *e);
void cdb_value_free(CdbValue *v);
bool cdb_value_compare(const CdbValue *a, const CdbValue *b, int *cmp, CdbError *e);
void cdb_value_print(FILE *out, const CdbValue *v);
#endif
