#ifndef CDB_EXECUTOR_H
#define CDB_EXECUTOR_H
#include "ast.h"
#include "cdb.h"
bool cdb_executor_run(Cdb *db, CdbQuery *q, CdbResult *result, CdbError *e);
#endif
