# Debugging, static analysis, and profiling

## Build and inspect

```bash
make debug
gdb --args ./bin/cdb-debug debug.cdb
```

Useful GDB commands:

```text
break cdb_execute
break cdb_page_insert
break cdb_wal_commit
run
bt
print db->tx.dirty_count
print db->page_count
```

GDB was not installed in the generation environment. These are supplied debugging instructions, not a recorded GDB session.

`CDB_LOG_LEVEL=DEBUG ./bin/cdb database.cdb` logs plans and transaction boundaries; TRACE also logs page allocation. ERROR/WARN/INFO/DEBUG/TRACE select thresholds. There is no hidden global logging configuration: it is stored per connection. `.stats` reports reads, writes, cache hits/misses, evictions, and private dirty pages. `.btree table column` prints actual nodes. `.check` checks page structure/committed checksums and B-Tree structure; it is not a comprehensive logical recovery or backup verifier.

## Sanitizers and Valgrind

```bash
make test-asan
make test-ubsan
make valgrind
```

ASan uses a separate build directory and non-PIE executables on the Linux sanitizer configuration to make shadow mapping predictable in constrained runtimes. This does not relax compiler warnings. UBSan uses `-fno-sanitize-recover=all`. See GCC's [instrumentation options](https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html) for toolchain semantics.

If LeakSanitizer cannot inspect `/proc` threads, record it as unavailable and run address checks with:

```bash
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 make test-asan
```

This is the path used for the recorded ASan pass. It is not proof of no leaks. Valgrind is an optional external debugging utility, not an engine dependency. The Valgrind target skips groups containing intentional `_exit` crash children, whose deliberately interrupted cleanup would otherwise produce irrelevant leak reports; normal SQL/ownership tests still run. Run the full recovery suite separately with `make test`.

`make lint` invokes GCC's static analyzer with actual compilation to `/dev/null`, plus the same warning gate. It is a useful additional check, not exhaustive verification. `make format` runs clang-format using `.clang-format` and requires it to be installed.

## Profiling

Use the release microbenchmark first. On Linux with performance tools and permission available:

```bash
make MODE=release benchmark ROWS=100000
perf record -g ./bin/benchmark-cdb-release 100000
perf report
```

Perf is optional and was not available for a recorded profile. A profiling build can also be made with explicit `CFLAGS`/`LDFLAGS`, but overriding CFLAGS replaces the default warning set, so preserve it when preparing reviewed changes. Benchmarks use `CLOCK_MONOTONIC` and verify results/plans; no timer result is hard-coded.

## Investigating corruption

Preserve both database and WAL. Record the exact error, OS/compiler, preceding SQL, and whether a commit returned success. Do not run ad-hoc truncation against a nonempty WAL. The engine distinguishes malformed complete logs from incomplete transaction prefixes under its documented sync assumptions. Source-level debugging should work on copies of closed files.
