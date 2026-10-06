#include "cdb/types.h"
#include <ctype.h>
#include <string.h>
const char *cdb_type_name(CdbType type) {
    static const char *const names[] = {"NULL", "INTEGER", "FLOAT", "TEXT", "BOOLEAN"};
    return (unsigned)type <= CDB_BOOLEAN ? names[type] : "INVALID";
}
uint16_t cdb_get16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8));
}
uint32_t cdb_get32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
uint64_t cdb_get64(const uint8_t *p) {
    return cdb_get32(p) | ((uint64_t)cdb_get32(p + 4) << 32);
}
void cdb_put16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}
void cdb_put32(uint8_t *p, uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
void cdb_put64(uint8_t *p, uint64_t v) {
    for (unsigned i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}
/* FNV-1a detects accidental damage. It is not a cryptographic integrity check. */ uint32_t
cdb_hash(uint32_t seed, const void *data, size_t n) {
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) {
        seed ^= p[i];
        seed *= UINT32_C(16777619);
    }
    return seed;
}
void cdb_seal(uint8_t *p) {
    cdb_put32(p + CDB_PAGE_END, cdb_hash(UINT32_C(2166136261), p, CDB_PAGE_END));
}
bool cdb_checksum(const uint8_t *p) {
    return cdb_get32(p + CDB_PAGE_END) == cdb_hash(UINT32_C(2166136261), p, CDB_PAGE_END);
}
bool cdb_identifier(const char *s) {
    size_t n = strlen(s);
    if (!n || n > CDB_NAME_MAX || !(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return false;
    for (size_t i = 0; i < n; i++) {
        if ((unsigned char)s[i] > 127u || !(isalnum((unsigned char)s[i]) || s[i] == '_'))
            return false;
    }
    return true;
}
void cdb_lower(char *s) {
    for (; *s; s++)
        *s = (char)tolower((unsigned char)*s);
}
