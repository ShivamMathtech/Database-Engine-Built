#include "benchmark.h"
bool benchmark_insert(Cdb *db, size_t rows) {
    double start = bench_now();
    if (!bench_sql(db, "BEGIN"))
        return false;
    for (size_t i = 0; i < rows; i++) {
        char sql[160];
        snprintf(sql, sizeof(sql), "INSERT INTO measurements VALUES (%zu,%zu,'benchmark row')", i,
                 i);
        if (!bench_sql(db, sql))
            return false;
        if ((i + 1) % 5000u == 0) {
            if (!bench_sql(db, "COMMIT"))
                return false;
            if (i + 1 < rows && !bench_sql(db, "BEGIN"))
                return false;
        }
    }
    if (rows % 5000u && !bench_sql(db, "COMMIT"))
        return false;
    double elapsed = bench_now() - start;
    printf("INSERT (batches of 5000)            %.6f s | %.0f rows/s\n", elapsed,
           elapsed > 0 ? (double)rows / elapsed : 0);
    return true;
}
