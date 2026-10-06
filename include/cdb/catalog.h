#ifndef CDB_CATALOG_H
#define CDB_CATALOG_H
#include "table.h"
CdbTable *cdb_catalog_find(Cdb *db, const char *name);
void cdb_catalog_free(Cdb *db);
bool cdb_catalog_load(Cdb *db, CdbError *e);
bool cdb_catalog_create(Cdb *db, const char *name, const CdbColumn *cols, uint16_t count,
                        CdbError *e);
bool cdb_catalog_save(Cdb *db, CdbTable *t, CdbError *e);
bool cdb_catalog_drop(Cdb *db, const char *name, CdbError *e);
#endif
