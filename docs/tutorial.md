# Learning lab: from SQL to durable bytes

## Lab 1 — Observe persistence

Build with `make`, create a new file using `examples/basic.sql`, exit, and reopen. The SQL result is an owned collection of typed values, while its persistence is encoded row data inside a slotted page. Find `cdb_row_serialize` and compare its explicit fields with the byte table in the storage-format document.

## Lab 2 — Follow a write

Start GDB on a debug build. Break at `cdb_execute`, `cdb_parse`, `cdb_table_insert`, `cdb_storage_edit`, and `cdb_wal_commit`. Insert a row. Inspect the query kind, schema column count, encoded row size, RID, and dirty-page count. Notice that normal INSERT does not call `pwrite` for a data page until commit.

## Lab 3 — Compare plans

Use a new database with `examples/users.sql`. Before CREATE INDEX, `EXPLAIN SELECT * FROM users WHERE age=24` reports a scan. After CREATE INDEX it reports an index lookup. The result stays equivalent, though row order is unspecified. `.stats` shows page/cache counters; the benchmark program also verifies `examined` for each plan.

## Lab 4 — Study deletion

Insert enough integer keys to split a B-Tree and use `.btree table column`. Delete keys near a separator. Read `borrow_left`, `borrow_right`, and `merge`. `test_btree.c` checks structural invariants through thousands of deterministic deletions, not merely whether one example returns a row.

## Lab 5 — Roll back DDL and data

Inside BEGIN, update a row, create a table, and create an index. Query your changes, then ROLLBACK and inspect `.tables`/`.schema`. The old disk pages never changed; in-memory catalog/indexes were reconstructed. Deliberately violate a primary key after a successful insert in the same transaction and verify the entire transaction was rolled back.

## Lab 6 — Learn the crash model

Read `tests/integration/test_recovery.c` and run `make test`. Its test-linked WAL code can terminate at five named commit stages. The normal CLI does not honor that environment switch. Explain why an incomplete log can be discarded before database application, and why a complete synced log must survive until database sync is complete.

## Lab 7 — Measure, do not guess

Run `make MODE=release benchmark ROWS=100000`. Record compiler, optimization, filesystem, row count, transaction batch size, query count, and machine. Repeat under similar conditions. The ratio is specific to an equality lookup against this data and this implementation; it is not a comparison with another database.

## Exercises

1. Add an IS NULL query test and explain why a NULL secondary key is absent from the B-Tree.
2. Add a parser feature only after specifying its AST ownership and executor type semantics.
3. Design overflow pages without invalidating the row/RID or recovery invariants.
4. Propose a free-page format and explain why monotonic-chain checks must change.
5. Sketch a streaming result API that makes cancellation and buffer ownership explicit.

Keep new storage features behind a format-version decision and deterministic corruption/recovery tests.
