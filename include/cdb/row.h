#ifndef CDB_ROW_H
#define CDB_ROW_H
#include "value.h"
typedef struct {
    uint16_t count;
    CdbValue values[CDB_MAX_COLUMNS];
} CdbRow;
typedef uint64_t CdbRid;
static inline CdbRid cdb_rid(uint32_t page, uint16_t slot) {
    return ((uint64_t)page << 16) | slot;
}
static inline uint32_t cdb_rid_page(CdbRid rid) {
    return (uint32_t)(rid >> 16);
}
static inline uint16_t cdb_rid_slot(CdbRid rid) {
    return (uint16_t)(rid & 65535u);
}
void cdb_row_free(CdbRow *row);
bool cdb_row_copy(CdbRow *dst, const CdbRow *src, CdbError *e);
size_t cdb_row_size(const CdbRow *row);
bool cdb_row_serialize(const CdbRow *row, uint8_t *out, size_t cap, size_t *used, CdbError *e);
bool cdb_row_deserialize(CdbRow *row, const uint8_t *data, size_t n, CdbError *e);
#endif
