#ifndef CDB_PAGE_H
#define CDB_PAGE_H
#include "error.h"
enum { CDB_PAGE_CATALOG = 1, CDB_PAGE_DATA = 2 };
void cdb_page_init(uint8_t *p, uint8_t type, uint32_t id, uint32_t owner);
bool cdb_page_validate(const uint8_t *p, uint32_t id, CdbError *e);
uint16_t cdb_page_slots(const uint8_t *p);
uint32_t cdb_page_next(const uint8_t *p);
void cdb_page_set_next(uint8_t *p, uint32_t next);
bool cdb_page_read(const uint8_t *p, uint16_t slot, const uint8_t **data, uint16_t *n, CdbError *e);
bool cdb_page_insert(uint8_t *p, const uint8_t *data, uint16_t n, uint16_t *slot, CdbError *e);
bool cdb_page_update(uint8_t *p, uint16_t slot, const uint8_t *data, uint16_t n, CdbError *e);
bool cdb_page_delete(uint8_t *p, uint16_t slot, CdbError *e);
void cdb_page_compact(uint8_t *p);
#endif
