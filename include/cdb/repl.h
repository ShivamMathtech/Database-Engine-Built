#ifndef CDB_REPL_H
#define CDB_REPL_H
#include "cdb.h"
int cdb_repl(Cdb *db, FILE *input, FILE *out, bool interactive);
void cdb_result_print(FILE *out, const CdbResult *result);
#endif
