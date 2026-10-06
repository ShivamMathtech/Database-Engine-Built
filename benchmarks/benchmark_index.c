#include "benchmark.h"
#include <string.h>
bool benchmark_index(Cdb *db, size_t rows, double scan_seconds) {
    double start = bench_now();
    if (!bench_sql(db, "CREATE INDEX value_idx ON measurements(value)"))
        return false;
    printf("CREATE INDEX                       %.6f s\n", bench_now() - start);
    char sql[128];
    snprintf(sql, sizeof(sql), "SELECT * FROM measurements WHERE value=%zu", rows / 2);
    start = bench_now();
    for (unsigned i = 0; i < 3000; i++) {
        CdbResult r = {0};
        CdbError e = {0};
        bool ok = cdb_execute(db, sql, &r, &e);
        bool valid =
            ok && r.row_count == 1 && r.examined == 1 && strstr(r.plan, "INDEX LOOKUP") != NULL;
        cdb_result_free(&r);
        if (!valid) {
            fputs("Unexpected index result/plan\n", stderr);
            return false;
        }
    }
    double elapsed = bench_now() - start;
    printf("INDEX LOOKUP (3000 queries)         %.6f s | %.3f us/query\n", elapsed,
           elapsed * 1000000 / 3000);
    if (elapsed > 0)
        printf("Measured mean scan/index ratio     %.2fx\n",
               (scan_seconds / 30) / (elapsed / 3000));
    return true;
}
