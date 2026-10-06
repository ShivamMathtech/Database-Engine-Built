#include "cdb/row.h"
#include <float.h>
#include <math.h>
#include <string.h>
_Static_assert(sizeof(double) == 8 && DBL_MANT_DIG == 53 && FLT_RADIX == 2 && DBL_MAX_EXP == 1024,
               "CDB requires IEEE-754 binary64 double");
void cdb_row_free(CdbRow *r) {
    for (uint16_t i = 0; i < r->count; i++)
        cdb_value_free(&r->values[i]);
    memset(r, 0, sizeof(*r));
}
bool cdb_row_copy(CdbRow *dst, const CdbRow *src, CdbError *e) {
    memset(dst, 0, sizeof(*dst));
    dst->count = src->count;
    for (uint16_t i = 0; i < src->count; i++) {
        if (!cdb_value_copy(&dst->values[i], &src->values[i], e)) {
            cdb_row_free(dst);
            return false;
        }
    }
    return true;
}
size_t cdb_row_size(const CdbRow *r) {
    size_t n = 2;
    for (uint16_t i = 0; i < r->count; i++) {
        const CdbValue *v = &r->values[i];
        n++;
        switch (v->type) {
        case CDB_NULL:
            break;
        case CDB_INTEGER:
        case CDB_FLOAT:
            n += 8;
            break;
        case CDB_BOOLEAN:
            n++;
            break;
        case CDB_TEXT:
            n += 4 + v->as.text.length;
            break;
        }
    }
    return n;
}
bool cdb_row_serialize(const CdbRow *r, uint8_t *out, size_t cap, size_t *used, CdbError *e) {
    if (!r->count || r->count > CDB_MAX_COLUMNS)
        return cdb_fail(e, CDB_ERR_TYPE, "Invalid column count");
    size_t n = cdb_row_size(r);
    if (n > cap || n > CDB_MAX_RECORD)
        return cdb_fail(e, CDB_ERR_LIMIT, "Row exceeds %u bytes", CDB_MAX_RECORD);
    cdb_put16(out, r->count);
    size_t at = 2;
    for (uint16_t i = 0; i < r->count; i++) {
        const CdbValue *v = &r->values[i];
        out[at++] = (uint8_t)v->type;
        switch (v->type) {
        case CDB_NULL:
            break;
        case CDB_INTEGER:
            cdb_put64(out + at, (uint64_t)v->as.integer);
            at += 8;
            break;
        case CDB_FLOAT: {
            uint64_t bits;
            if (!isfinite(v->as.real))
                return cdb_fail(e, CDB_ERR_TYPE, "Non-finite FLOAT");
            memcpy(&bits, &v->as.real, 8);
            cdb_put64(out + at, bits);
            at += 8;
            break;
        }
        case CDB_BOOLEAN:
            out[at++] = (uint8_t)v->as.boolean;
            break;
        case CDB_TEXT:
            cdb_put32(out + at, (uint32_t)v->as.text.length);
            at += 4;
            memcpy(out + at, v->as.text.data, v->as.text.length);
            at += v->as.text.length;
            break;
        default:
            return cdb_fail(e, CDB_ERR_TYPE, "Invalid type tag");
        }
    }
    *used = at;
    return true;
}
bool cdb_row_deserialize(CdbRow *r, const uint8_t *p, size_t n, CdbError *e) {
    memset(r, 0, sizeof(*r));
    if (n < 2 || n > CDB_MAX_RECORD)
        goto corrupt;
    r->count = cdb_get16(p);
    if (!r->count || r->count > CDB_MAX_COLUMNS) {
        r->count = 0;
        goto corrupt;
    }
    size_t at = 2;
    for (uint16_t i = 0; i < r->count; i++) {
        CdbValue *v = &r->values[i];
        if (at >= n)
            goto corrupt;
        uint8_t type = p[at++];
        if (type > CDB_BOOLEAN)
            goto corrupt;
        v->type = (CdbType)type;
        if (type == CDB_INTEGER || type == CDB_FLOAT) {
            if (n - at < 8)
                goto corrupt;
            uint64_t bits = cdb_get64(p + at);
            at += 8;
            if (type == CDB_INTEGER)
                v->as.integer = bits <= INT64_MAX ? (int64_t)bits : -1 - (int64_t)(~bits);
            else {
                memcpy(&v->as.real, &bits, 8);
                if (!isfinite(v->as.real))
                    goto corrupt;
            }
        } else if (type == CDB_BOOLEAN) {
            if (at >= n || p[at] > 1)
                goto corrupt;
            v->as.boolean = p[at++] != 0;
        } else if (type == CDB_TEXT) {
            if (n - at < 4)
                goto corrupt;
            size_t len = cdb_get32(p + at);
            at += 4;
            if (len > n - at || memchr(p + at, 0, len))
                goto corrupt;
            if (!cdb_text(v, (const char *)p + at, len, e)) {
                cdb_row_free(r);
                return false;
            }
            at += len;
        }
    }
    if (at != n)
        goto corrupt;
    return true;
corrupt:
    cdb_row_free(r);
    return cdb_fail(e, CDB_ERR_CORRUPT, "Malformed serialized row");
}
