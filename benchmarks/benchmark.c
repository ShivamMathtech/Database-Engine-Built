#include "benchmark.h"
#include <errno.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
double bench_now(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t))
        return 0;
    return (double)t.tv_sec + (double)t.tv_nsec / 1000000000.0;
}
bool bench_sql(Cdb *db, const char *sql) {
    CdbResult r = {0};
    CdbError e = {0};
    bool ok = cdb_execute(db, sql, &r, &e);
    if (!ok)
        fprintf(stderr, "Benchmark: %s\n", e.message);
    cdb_result_free(&r);
    return ok;
}
static bool mutations(Cdb *db) {
    const char *names[] = {"UPDATE 1000 primary-key matches", "DELETE 1000 primary-key matches"};
    for (size_t k = 0; k < 2; k++) {
        double start = bench_now();
        if (!bench_sql(db, "BEGIN"))
            return false;
        for (size_t i = 0; i < 1000; i++) {
            char sql[160];
            /* Two fixed formats; avoid unchecked user format strings. */ if (k == 0)
                snprintf(sql, sizeof(sql), "UPDATE measurements SET value=value+1 WHERE id=%zu", i);
            else
                snprintf(sql, sizeof(sql), "DELETE FROM measurements WHERE id=%zu", i);
            if (!bench_sql(db, sql))
                return false;
        }
        if (!bench_sql(db, "COMMIT"))
            return false;
        printf("%-34s %.6f s (one durable transaction)\n", names[k], bench_now() - start);
    }
    return true;
}
int main(int argc, char **argv) {
    size_t rows = 100000;
    if (argc > 2)
        return 2;
    if (argc == 2) {
        errno = 0;
        char *end;
        unsigned long n = strtoul(argv[1], &end, 10);
        if (errno || *end || n < 1000 || n > 1000000) {
            fputs("Rows must be 1000..1000000\n", stderr);
            return 2;
        }
        rows = (size_t)n;
    }
    char path[] = "/tmp/cdb-benchmark-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0)
        return 1;
    (void)close(fd);
    Cdb *db = NULL;
    CdbError e = {0};
    bool ok = cdb_open(path, &db, &e);
    double scan = 0;
    printf("CDB v%s benchmark | rows=%zu | CLOCK_MONOTONIC\n", CDB_VERSION, rows);
    if (ok)
        ok = bench_sql(
            db, "CREATE TABLE measurements (id INTEGER PRIMARY KEY, value INTEGER, payload TEXT)");
    if (ok)
        ok = benchmark_insert(db, rows);
    if (ok)
        ok = benchmark_select(db, rows, &scan);
    if (ok)
        ok = benchmark_index(db, rows, scan);
    if (ok)
        ok = mutations(db);
    cdb_close(db);
    (void)unlink(path);
    char wal[128];
    snprintf(wal, sizeof(wal), "%s.wal", path);
    (void)unlink(wal);
    if (!ok) {
        fprintf(stderr, "Benchmark failed: %s\n", e.message);
        return 1;
    }
    puts("Measurements include parsing/result allocation; scan and index queries are identical.\n"
         "Scan timing precedes index creation. Results describe this run, not a portable "
         "guarantee.");
    return 0;
}
