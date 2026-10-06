#ifndef CDB_TYPES_H
#define CDB_TYPES_H
#include "common.h"
typedef enum { CDB_NULL, CDB_INTEGER, CDB_FLOAT, CDB_TEXT, CDB_BOOLEAN } CdbType;
const char *cdb_type_name(CdbType type);
#endif
