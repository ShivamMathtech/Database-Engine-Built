#include "cdb/page.h"
#include <string.h>
void cdb_page_init(uint8_t *p, uint8_t type, uint32_t id, uint32_t owner) {
    memset(p, 0, CDB_PAGE_SIZE);
    p[0] = type;
    cdb_put16(p + 6, CDB_PAGE_END);
    cdb_put32(p + 8, CDB_NO_PAGE);
    cdb_put32(p + 12, owner);
    cdb_put32(p + 16, id);
}
uint16_t cdb_page_slots(const uint8_t *p) {
    return cdb_get16(p + 2);
}
uint32_t cdb_page_next(const uint8_t *p) {
    return cdb_get32(p + 8);
}
void cdb_page_set_next(uint8_t *p, uint32_t next) {
    cdb_put32(p + 8, next);
}
bool cdb_page_validate(const uint8_t *p, uint32_t id, CdbError *e) {
    uint16_t slots = cdb_page_slots(p), end = cdb_get16(p + 6);
    size_t start = CDB_PAGE_HEADER + 4u * slots;
    if ((p[0] != CDB_PAGE_DATA && p[0] != CDB_PAGE_CATALOG) || cdb_get32(p + 16) != id ||
        start > end || end > CDB_PAGE_END)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid header on page %u", id);
    uint8_t used[CDB_PAGE_END] = {0};
    for (uint16_t i = 0; i < slots; i++) {
        const uint8_t *s = p + CDB_PAGE_HEADER + 4u * i;
        uint16_t off = cdb_get16(s), len = cdb_get16(s + 2);
        if (!len) {
            if (off)
                return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid tombstone on page %u", id);
            continue;
        }
        if (off < end || (size_t)off + len > CDB_PAGE_END)
            return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid slot on page %u", id);
        for (size_t j = off; j < (size_t)off + len; j++) {
            if (used[j])
                return cdb_fail(e, CDB_ERR_CORRUPT, "Overlapping records on page %u", id);
            used[j] = 1;
        }
    }
    return true;
}
bool cdb_page_read(const uint8_t *p, uint16_t slot, const uint8_t **data, uint16_t *n,
                   CdbError *e) {
    if (slot >= cdb_page_slots(p))
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Invalid slot %u", (unsigned)slot);
    const uint8_t *s = p + CDB_PAGE_HEADER + 4u * slot;
    *n = cdb_get16(s + 2);
    if (!*n) {
        *data = NULL;
        return true;
    }
    uint16_t off = cdb_get16(s);
    if (off < CDB_PAGE_HEADER + 4u * cdb_page_slots(p) || (size_t)off + *n > CDB_PAGE_END)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid record boundary");
    *data = p + off;
    return true;
}
void cdb_page_compact(uint8_t *p) {
    uint8_t tmp[CDB_PAGE_SIZE] = {0};
    memcpy(tmp, p, CDB_PAGE_HEADER);
    uint16_t end = CDB_PAGE_END;
    for (uint16_t i = 0; i < cdb_page_slots(p); i++) {
        const uint8_t *s = p + CDB_PAGE_HEADER + 4u * i;
        uint16_t len = cdb_get16(s + 2);
        if (len) {
            end = (uint16_t)(end - len);
            memcpy(tmp + end, p + cdb_get16(s), len);
        }
        cdb_put16(tmp + CDB_PAGE_HEADER + 4u * i, len ? end : 0);
        cdb_put16(tmp + CDB_PAGE_HEADER + 4u * i + 2, len);
    }
    cdb_put16(tmp + 6, end);
    memcpy(p, tmp, CDB_PAGE_SIZE);
}
static bool insert_at(uint8_t *p, uint16_t slot, const uint8_t *data, uint16_t n, bool new_slot,
                      CdbError *e) {
    uint16_t slots = cdb_page_slots(p);
    size_t occupied = 0;
    for (uint16_t i = 0; i < slots; i++)
        occupied += cdb_get16(p + CDB_PAGE_HEADER + 4u * i + 2);
    size_t count = (size_t)slots + (new_slot ? 1u : 0u);
    if (!n || CDB_PAGE_HEADER + 4u * count + occupied + n > CDB_PAGE_END)
        return cdb_fail(e, CDB_ERR_LIMIT, "Page has insufficient free space");
    cdb_page_compact(p);
    uint16_t end = (uint16_t)(cdb_get16(p + 6) - n);
    memcpy(p + end, data, n);
    cdb_put16(p + CDB_PAGE_HEADER + 4u * slot, end);
    cdb_put16(p + CDB_PAGE_HEADER + 4u * slot + 2, n);
    cdb_put16(p + 6, end);
    if (new_slot)
        cdb_put16(p + 2, (uint16_t)(slots + 1));
    return true;
}
bool cdb_page_insert(uint8_t *p, const uint8_t *data, uint16_t n, uint16_t *slot, CdbError *e) {
    uint16_t slots = cdb_page_slots(p);
    *slot = slots;
    for (uint16_t i = 0; i < slots; i++)
        if (!cdb_get16(p + CDB_PAGE_HEADER + 4u * i + 2)) {
            *slot = i;
            break;
        }
    return insert_at(p, *slot, data, n, *slot == slots, e);
}
bool cdb_page_delete(uint8_t *p, uint16_t slot, CdbError *e) {
    if (slot >= cdb_page_slots(p) || !cdb_get16(p + CDB_PAGE_HEADER + 4u * slot + 2))
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Record is deleted or missing");
    memset(p + CDB_PAGE_HEADER + 4u * slot, 0, 4);
    return true;
}
bool cdb_page_update(uint8_t *p, uint16_t slot, const uint8_t *data, uint16_t n, CdbError *e) {
    uint8_t tmp[CDB_PAGE_SIZE];
    memcpy(tmp, p, sizeof(tmp));
    CDB_TRY(cdb_page_delete(tmp, slot, e));
    CDB_TRY(insert_at(tmp, slot, data, n, false, e));
    memcpy(p, tmp, sizeof(tmp));
    return true;
}
