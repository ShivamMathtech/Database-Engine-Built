#ifndef CDB_STORAGE_H
#define CDB_STORAGE_H
#include "table.h"
bool cdb_storage_read(Cdb *db, uint32_t id, uint8_t *out, CdbError *e);
/* Borrowed page pointer remains valid until transaction ends. */ bool
cdb_storage_edit(Cdb *db, uint32_t id, uint8_t **out, CdbError *e);
bool cdb_storage_allocate(Cdb *db, uint8_t type, uint32_t owner, uint32_t *id, CdbError *e);
bool cdb_storage_header(Cdb *db, CdbError *e);
#endif
