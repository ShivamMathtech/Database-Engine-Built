# Recommended Git commit sequence

This is a learning/development sequence, not a fabricated record of commits already made.

1. Initialize project identity, license, style, and scope.
2. Define architecture, file format, invariants, and public APIs.
3. Add build modes and structured errors.
4. Implement typed values and ownership tests.
5. Implement explicit row serialization and corruption checks.
6. Add tokenizer with token positions and malformed-input tests.
7. Add AST ownership and precedence parser.
8. Implement slotted pages and compaction.
9. Add positioned pager I/O, checksums, and locking.
10. Add committed-page clock buffer pool with pinning tests.
11. Add transaction-private page maps and allocation.
12. Add catalog/table page chains and schema validation.
13. Implement B-Tree search, insertion, and splitting.
14. Implement B-Tree deletion, borrow/merge, and integrity tests.
15. Integrate primary/secondary indexes and reconstruction.
16. Implement binding, expressions, plans, and SELECT.
17. Add INSERT/UPDATE/DELETE and candidate collection.
18. Add autocommit and explicit transaction rollback.
19. Implement full-page redo log and recovery validation.
20. Add test-only crash points and subprocess recovery tests.
21. Add REPL formatting, metadata, examples, and C embedding.
22. Add randomized parser/B-Tree tests and 100k stress workloads.
23. Add actual benchmarks, sanitizers, static analysis, and CI.
24. Complete tutorials, platform limits, binary-format docs, and audit report.
25. Package v0.1.0 with a reproducible sample database.

Each commit should compile, preserve earlier tests, and explain its new invariant. Avoid one commit that adds an API declaration while leaving all callers/linkage broken for the next milestone.
