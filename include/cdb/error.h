#ifndef CDB_ERROR_H
#define CDB_ERROR_H
#include "common.h"
typedef enum {
    CDB_OK,
    CDB_ERR_IO,
    CDB_ERR_MEMORY,
    CDB_ERR_SYNTAX,
    CDB_ERR_TYPE,
    CDB_ERR_CONSTRAINT,
    CDB_ERR_NOT_FOUND,
    CDB_ERR_EXISTS,
    CDB_ERR_CORRUPT,
    CDB_ERR_LIMIT,
    CDB_ERR_TRANSACTION,
    CDB_ERR_BUSY,
    CDB_ERR_UNSUPPORTED,
    CDB_ERR_INTERNAL
} CdbCode;
typedef struct {
    CdbCode code;
    size_t position;
    char message[512];
} CdbError;
void cdb_error_clear(CdbError *e);
bool cdb_fail(CdbError *e, CdbCode code, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
const char *cdb_code_name(CdbCode code);
#endif
