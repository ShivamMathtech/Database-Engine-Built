# Measured benchmark run

Executed 2026-10-05 using the supplied `benchmark-cdb-release` executable, with 100,000 rows. These are real observed measurements, not expected/theoretical figures.

Environment: Linux 6.18.44 x86_64; GCC 13.3.0 (Ubuntu build); C17; `-O2 -g`; local temporary database on an **overlay filesystem** in the execution container. No dedicated storage hardware was characterized. The standalone measured run was separate from the earlier stress execution.

```bash
make MODE=release bin/benchmark-cdb-release
./bin/benchmark-cdb-release 100000
```

Exact executable output:

```text
CDB v0.1.0 benchmark | rows=100000 | CLOCK_MONOTONIC
INSERT (batches of 5000)            0.193446 s | 516939 rows/s
FULL TABLE SCAN (30 queries)        0.520324 s | 17.344 ms/query
CREATE INDEX                       0.031895 s
INDEX LOOKUP (3000 queries)         0.008464 s | 2.821 us/query
Measured mean scan/index ratio     6147.13x
UPDATE 1000 primary-key matches    0.008685 s (one durable transaction)
DELETE 1000 primary-key matches    0.003577 s (one durable transaction)
Measurements include parsing/result allocation; scan and index queries are identical.
Scan timing precedes index creation. Results describe this run, not a portable guarantee.
```

## Interpret the numbers narrowly

This compares one equality predicate on a distinct integer value, before/after its index is created. The query returns one row; the scan visits 100,000 rows. The scan is repeated 30 times and index lookup 3,000 times, and the ratio uses per-query means rather than mismatched totals. SQL parsing and result allocation are included. The primary-key index already exists during INSERT; the secondary index is built later. Secondary-index creation currently rebuilds every tree.

INSERT uses 5,000-row transactions; UPDATE and DELETE each batch 1,000 statements into one transaction. These are **not** per-row durable-commit latencies. `fsync` calls were executed, but container-overlay timings say little about physical-media durability or throughput. SELECTs run against warm operating-system data. There is no variance/confidence interval, concurrency, cold-cache comparison, recovery-speed benchmark, or comparison with another database. Re-run locally rather than using the displayed ratio as a product claim.
