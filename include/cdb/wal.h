#ifndef CDB_WAL_H
#define CDB_WAL_H
#include "table.h"
bool cdb_wal_recover(Cdb *db, CdbError *e);
bool cdb_wal_commit(Cdb *db, CdbError *e);
#endif
