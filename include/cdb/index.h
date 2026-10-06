#ifndef CDB_INDEX_H
#define CDB_INDEX_H
#include "table.h"
bool cdb_index_rebuild(Cdb *db, CdbError *e);
bool cdb_index_create(Cdb *db, CdbTable *t, const char *name, const char *column, CdbError *e);
bool cdb_index_add(CdbTable *t, const CdbRow *row, CdbRid rid, CdbError *e);
void cdb_index_remove(CdbTable *t, const CdbRow *row, CdbRid rid);
bool cdb_index_check(const CdbTable *t, const CdbRow *row, CdbRid except, CdbError *e);
#endif
