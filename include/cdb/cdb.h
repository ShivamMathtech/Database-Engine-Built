#ifndef CDB_H
#define CDB_H
#include "row.h"
typedef struct Cdb Cdb;
typedef struct {
    uint16_t column_count;
    char columns[CDB_MAX_COLUMNS][CDB_NAME_MAX + 1];
    CdbRow *rows;
    size_t row_count, row_capacity;
    uint64_t affected, examined;
    char plan[128], message[192];
} CdbResult;
/* One connection/thread/process at a time per file. close discards an open transaction. */ bool
cdb_open(const char *path, Cdb **db, CdbError *e);
void cdb_close(Cdb *db);
bool cdb_execute(Cdb *db, const char *sql, CdbResult *result, CdbError *e);
void cdb_result_free(CdbResult *result);
bool cdb_meta(Cdb *db, const char *command, FILE *out, CdbError *e);
#endif
