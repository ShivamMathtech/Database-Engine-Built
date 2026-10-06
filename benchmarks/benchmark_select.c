#include "benchmark.h"
#include <string.h>
bool benchmark_select(Cdb *db, size_t rows, double *seconds) {
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT * FROM measurements WHERE value=%zu", rows / 2);
    double start = bench_now();
    for (unsigned i = 0; i < 30; i++) {
        CdbResult result = {0};
        CdbError e = {0};
        bool ok = cdb_execute(db, sql, &result, &e);
        bool valid = ok && result.row_count == 1 && result.examined == rows &&
                     strstr(result.plan, "FULL TABLE SCAN") != NULL;
        cdb_result_free(&result);
        if (!valid) {
            fputs("Unexpected full-scan result/plan\n", stderr);
            return false;
        }
    }
    *seconds = bench_now() - start;
    printf("FULL TABLE SCAN (30 queries)        %.6f s | %.3f ms/query\n", *seconds,
           *seconds * 1000 / 30);
    return true;
}
