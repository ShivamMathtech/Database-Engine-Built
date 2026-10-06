# Roadmap and capability status

Completed means implemented and exercised by the supplied tests unless noted otherwise. It does not mean enterprise readiness.

## v0.1.0

- [x] C17 modular CLI and embedded API
- [x] Lexer, explicit AST, precedence parser, bounded SQL input
- [x] Typed values, owned TEXT, binary row codec
- [x] Schemas and catalog; CREATE/DROP, INSERT/SELECT/UPDATE/DELETE
- [x] Persistent slotted pages, compaction, positioned I/O
- [x] Committed clock cache and transaction-private dirty-page tracking
- [x] Manual B-Tree insertion/search/duplicate visits/deletion/split/borrow/merge
- [x] Integer indexes and equality planning, EXPLAIN SELECT
- [x] BEGIN/COMMIT/ROLLBACK, autocommit, transactional catalog changes
- [x] Full-page redo WAL and process-crash recovery tests
- [x] Deterministic unit/integration/stress tests and real benchmarks
- [x] ASan address checks and UBSan execution in the supplied environment
- [x] Build modes, scripts, documented optional Valgrind/format tools
- [x] CI workflow configuration (remote CI execution is not verified)

## Next: strengthen storage validation

- [ ] Allocation-failure injection and arbitrary short-write/fsync error tests
- [ ] Crash during recovery, more torn-write permutations, filesystem fault harness
- [ ] LeakSanitizer/Valgrind validation on an unrestricted runner
- [ ] Long-running randomized model-based SQL/storage fuzzing
- [ ] Physical power-loss validation on supported hardware/filesystems
- [ ] Offline backup/restore verification and page inspection tool

## Storage extensions

- [ ] Persistent B-Tree nodes and recoverable index-page format
- [ ] Free list, vacuum, dropped-page reclamation, overflow TEXT
- [ ] Streaming result cursors and early LIMIT termination
- [ ] Range-index cursors, cost-based plans, text/composite indexes
- [ ] Statement savepoints and incremental rollback
- [ ] Format migration/version-upgrade tools

## SQL and execution

- [ ] Prepared statements / parameter binding
- [ ] ORDER BY, aggregates, GROUP BY, JOIN, subqueries
- [ ] ALTER TABLE, DROP INDEX, foreign keys
- [ ] Unicode-aware collation (current TEXT is bytewise)

## Platform and concurrency

- [ ] Native Win32 backend (STATUS: NOT IMPLEMENTED; POSIX backend currently uses WSL on Windows)
- [ ] Multiple readers/writers and well-defined isolation
- [ ] MVCC, deadlock handling, cancellation, parallel execution
- [ ] Network protocol, server, authentication and access controls

**STATUS: NOT IMPLEMENTED** applies to all unchecked items. Their absence is intentional: persistence correctness, memory ownership, and observable validation take precedence over feature count. A network server is deferred until parameter binding, streaming, resource controls, and a concurrency model are stable.
