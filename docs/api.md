# Embedding CDB in a C program

`include/cdb/cdb.h` exposes an opaque connection, typed result rows, errors, and four main operations:

```c
bool cdb_open(const char *path, Cdb **out, CdbError *error);
bool cdb_execute(Cdb *db, const char *sql, CdbResult *out, CdbError *error);
void cdb_result_free(CdbResult *result);
void cdb_close(Cdb *db);
```

`cdb_execute` accepts exactly one statement and initializes its output; the caller must free a previous result before reusing that object. Values in returned rows are independent owned copies. A result can outlive the connection. A failed call also leaves a safely freeable result. Error pointers may be NULL, but supplying one is strongly recommended. The public API checks its connection/output/path inputs; internal module APIs expect documented valid structures.

See the actual [embedding example](../examples/embedding.c). Compile from the repository root:

```bash
make
cc -std=c17 -Iinclude examples/embedding.c build/debug/libcdb.a -lm -o bin/embedding
./bin/embedding
```

The example creates `embedded-demo.cdb`, runs an INSERT inside a transaction, selects its row, then rolls back, leaving the schema for repeated runs. It does not assume a nonexistent network or client SDK. For parameterized external input, a prepared-statement API is a roadmap prerequisite; do not construct SQL by blindly interpolating strings.

## Error and transaction contract

- `CdbError.code` identifies category; `.message` supplies context; `.position` is a SQL byte offset when provided.
- SQL writes autocommit unless BEGIN is active. A failed write rolls back the entire transaction.
- Syntax/read errors leave an explicit transaction active. Nested BEGIN fails.
- An uncertain commit I/O failure poisons the connection. Close/reopen and inspect state; retrying blindly can duplicate an application action.
- Calls are synchronous and single-threaded. One connection holds the file's advisory lock for its entire lifetime.
- No cancellation, async callbacks, read-only connection mode, parameter binding, or streaming cursor API is implemented.
