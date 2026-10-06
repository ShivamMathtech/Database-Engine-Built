#ifndef CDB_BENCHMARK_H
#define CDB_BENCHMARK_H
#include "cdb/cdb.h"
bool bench_sql(Cdb *db, const char *sql);
double bench_now(void);
bool benchmark_insert(Cdb *db, size_t rows);
bool benchmark_select(Cdb *db, size_t rows, double *seconds);
bool benchmark_index(Cdb *db, size_t rows, double scan_seconds);
#endif
