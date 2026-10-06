#include "cdb/value.h"
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
CdbValue cdb_null(void) {
    CdbValue v = {0};
    return v;
}
CdbValue cdb_integer(int64_t n) {
    CdbValue v = {.type = CDB_INTEGER};
    v.as.integer = n;
    return v;
}
CdbValue cdb_float(double n) {
    CdbValue v = {.type = CDB_FLOAT};
    v.as.real = n;
    return v;
}
CdbValue cdb_boolean(bool b) {
    CdbValue v = {.type = CDB_BOOLEAN};
    v.as.boolean = b;
    return v;
}
bool cdb_text(CdbValue *v, const char *s, size_t n, CdbError *e) {
    *v = cdb_null();
    if (n > CDB_MAX_RECORD || memchr(s, 0, n))
        return cdb_fail(e, CDB_ERR_LIMIT, "Text is too long or contains NUL bytes");
    char *copy = malloc(n + 1);
    if (!copy)
        return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate text");
    memcpy(copy, s, n);
    copy[n] = '\0';
    v->type = CDB_TEXT;
    v->as.text.data = copy;
    v->as.text.length = n;
    return true;
}
bool cdb_value_copy(CdbValue *dst, const CdbValue *src, CdbError *e) {
    if (src->type == CDB_TEXT)
        return cdb_text(dst, src->as.text.data, src->as.text.length, e);
    *dst = *src;
    return true;
}
void cdb_value_free(CdbValue *v) {
    if (v->type == CDB_TEXT)
        free(v->as.text.data);
    *v = cdb_null();
}
bool cdb_value_compare(const CdbValue *a, const CdbValue *b, int *cmp, CdbError *e) {
    if ((a->type == CDB_INTEGER || a->type == CDB_FLOAT) &&
        (b->type == CDB_INTEGER || b->type == CDB_FLOAT)) {
        /* Keep integer comparisons exact, including both int64 endpoints. */ if (a->type ==
                                                                                      CDB_INTEGER &&
                                                                                  b->type ==
                                                                                      CDB_INTEGER) {
            *cmp = (a->as.integer > b->as.integer) - (a->as.integer < b->as.integer);
            return true;
        }
        /* Avoid rounding an int64 into double before comparison. The bounds
         * make the float-to-integer cast defined even on platforms whose long
         * double has only 53 bits of precision. */
        const CdbValue *iv = a->type == CDB_INTEGER ? a : b;
        const CdbValue *fv = a->type == CDB_FLOAT ? a : b;
        if (a->type == CDB_FLOAT && b->type == CDB_FLOAT) {
            if (!isfinite(a->as.real) || !isfinite(b->as.real))
                return cdb_fail(e, CDB_ERR_TYPE, "Non-finite numbers are unsupported");
            *cmp = (a->as.real > b->as.real) - (a->as.real < b->as.real);
            return true;
        }
        double number = fv->as.real;
        int result;
        if (!isfinite(number))
            return cdb_fail(e, CDB_ERR_TYPE, "Non-finite numbers are unsupported");
        if (number >= 9223372036854775808.0)
            result = -1;
        else if (number < -9223372036854775808.0)
            result = 1;
        else {
            int64_t integral = (int64_t)number;
            if (iv->as.integer != integral)
                result = (iv->as.integer > integral) ? 1 : -1;
            else
                result = ((double)integral > number) - ((double)integral < number);
        }
        *cmp = a->type == CDB_INTEGER ? result : -result;
        return true;
    }
    if (a->type != b->type || a->type == CDB_NULL)
        return cdb_fail(e, CDB_ERR_TYPE, "Cannot compare %s and %s", cdb_type_name(a->type),
                        cdb_type_name(b->type));
    if (a->type == CDB_TEXT) {
        int n = strcmp(a->as.text.data, b->as.text.data);
        *cmp = (n > 0) - (n < 0);
        return true;
    }
    if (a->type == CDB_BOOLEAN) {
        *cmp = (int)a->as.boolean - (int)b->as.boolean;
        return true;
    }
    return cdb_fail(e, CDB_ERR_TYPE, "Invalid value type");
}
void cdb_value_print(FILE *out, const CdbValue *v) {
    switch (v->type) {
    case CDB_NULL:
        fputs("NULL", out);
        break;
    case CDB_INTEGER:
        fprintf(out, "%" PRId64, v->as.integer);
        break;
    case CDB_FLOAT:
        fprintf(out, "%.17g", v->as.real);
        break;
    case CDB_BOOLEAN:
        fputs(v->as.boolean ? "TRUE" : "FALSE", out);
        break;
    case CDB_TEXT:
        for (size_t i = 0; i < v->as.text.length; i++) {
            unsigned char c = (unsigned char)v->as.text.data[i];
            if (c == '\n')
                fputs("\\n", out);
            else if (c == '\r')
                fputs("\\r", out);
            else if (c == '\t')
                fputs("\\t", out);
            else if (c < 32u || c == 127u)
                fprintf(out, "\\x%02x", (unsigned)c);
            else
                fputc(c, out);
        }
        break;
    }
}
