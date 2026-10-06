# Validation report — v0.1.0

Validation was executed in the available environment on **2026-10-05**. The results below report actual tool execution. Tests are evidence for the exercised cases, not a proof of all database correctness or hardware durability.

## Environment

- Linux 6.18.44, x86_64, overlay filesystem.
- GCC 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04).
- GNU Make; C17; warnings include conversion/shadow/format checks and `-Werror`.
- clang-format 18.1.8 was installed as a temporary development utility and executed. It is not bundled or required by the engine.
- Compiler/runtime dependencies: libc, libm, and POSIX. No external database or parser library.

## Executed results

| Check | Command or method | Actual outcome |
|---|---|---|
| Strict debug compile/link | `make test` | PASS; all 11 groups, no skipped groups, CLI checks |
| Strict release compile/link | `make MODE=release test` | PASS; all 11 groups, no skipped groups, CLI checks |
| Large workload | `make MODE=release stress ROWS=100000` | PASS; 100,000 inserted/reopened, 2,000 PK queries, 100 duplicate-key queries, rollback and 1,000 deletes |
| AddressSanitizer | `ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 make test-asan` | PASS; 11 groups, CLI, 10,000-row stress; **leak detection disabled** |
| UndefinedBehaviorSanitizer | `make test-ubsan` | PASS; 11 groups, CLI, 10,000-row stress |
| GCC static analyzer | `make lint` (compiles with `-fanalyzer -c` to `/dev/null`) | PASS; no emitted warnings/errors |
| Formatting | `make format`, clang-format 18.1.8 | EXECUTED |
| Process crash recovery | Forked child stops at 5 WAL/application stages | PASS; expected old/new state, empty WAL after recovery, second reopen |
| Corruption defenses | Damaged page, WAL record, unknown format, hard-linked WAL/database | PASS; errors instead of data acceptance/truncation |
| C embedding example | Compile against `libcdb.a`, run twice | PASS; both runs print `Hello, Shivam!` and roll back the demo row |
| Example database | Generate with `university.sql`, open separately | PASS; 5 students, 3 courses, 2 faculty |
| Benchmarks | Release executable, 100,000 rows | EXECUTED; real timings in `benchmark-results.md` |
| Valgrind-compatible skip selection | `CDB_SKIP_CRASH_TESTS=1 bin/test-cdb-debug` | PASS; 9 groups run, 2 intentional-crash groups explicitly skipped; **not a Valgrind run** |

## Coverage details

The eleven test groups are values/row codec, lexer, parser/malformed SQL, pages/pager/cache, B-Tree, tables/catalog/constraints, transactions, queries/planner/index maintenance, persistence/relocation, crash recovery, and corrupt files/WAL.

Notable workloads include 10,000 deterministic random SQL byte strings; 12,000 sequential B-Tree keys followed by shuffled deletion and repeated structural validation; a second shuffled insertion; 1,000 duplicate-valued index entries; growing a stored TEXT value to 3,500 bytes; transaction rollback of data, schemas, and indexes; pinned-cache exhaustion; and independent CLI processes demonstrating restart persistence and rollback-on-exit.

The source-linked test library alone enables crash hooks. Production code does not honor the crash environment variable. A separate subprocess shell script exercises multiline SQL, quoted semicolons, comments, metadata, nonzero batch errors, and restart. Test assertions are not compiled out in release builds.

## Not verified / not implemented

- **LeakSanitizer: NOT VERIFIED IN THIS ENVIRONMENT.** With `detect_leaks=1`, the sanitizer failed while reading `/proc/<pid>/task`, reporting that it could not enumerate threads. The subsequent ASan pass explicitly disabled leak checking. No leak-free claim is made.
- **Valgrind, Clang compiler, GDB, perf, Windows/WSL and macOS: NOT VERIFIED IN THIS ENVIRONMENT.** Commands/configuration are supplied; they were not executed here.
- **GitHub Actions: NOT VERIFIED IN THIS ENVIRONMENT.** The supplied workflow references real build/test targets; it has not been run on a published repository.
- Physical power failure, arbitrary I/O failures/disk-full, allocation-failure injection, concurrent access beyond the connection-lock rejection test, large-format maximums, and recovery-interruption permutations remain unverified or roadmap work.
- Native Windows backend, persistent B-Tree pages, free-list/vacuum, streaming results, JOIN/aggregates/MVCC/server mode are NOT IMPLEMENTED, as documented in the roadmap.

## Evidence files

`docs/validation-logs/` contains debug/release/sanitizer/analyzer output, the blocked leak-check diagnostic, standalone benchmark output, and sample reopen output. Working-directory prefixes in captured build logs are normalized to `<project-root>`; test results and timing text are unchanged. Parallel build/test output can interleave lines; group completion and process exit status were checked.

## Package audit

The ZIP contains portable source, headers, tests, examples, documentation, build configuration, and the generated sample database. Build outputs and temporary developer utilities are excluded. Clean extraction audit: **PASS**. The source ZIP was extracted into a separate directory with no build outputs; `make test` compiled from scratch and passed all 11 groups plus CLI checks. `sha256sum -c MANIFEST.sha256` verified the packaged files. The packaged sample database returned the expected three students older than 23 and passed `.check`. See `validation-logs/clean-extraction.txt`. The final packaging update adds only this audit text/log and regenerated hashes; engine source is identical to the clean-build source.
