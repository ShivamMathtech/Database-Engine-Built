# Changelog

## v0.1.0

- Initial modular C17 engine and embedded API.
- Typed values, lexer, precedence parser, owned AST, binding, and SQL CRUD.
- Explicit binary row codec, persistent slotted pages, catalog, pager, and clock cache.
- Integer B-Tree indexes with split/borrow/merge deletion and equality planning.
- Private-page transactions, rollback of schema/data, and checksummed redo WAL.
- Process-crash recovery tests, corruption checks, deterministic tests and stress workloads.
- Real benchmark executable, strict builds, sanitizer targets, static analysis, CI configuration.
- CLI metadata/tree/debug commands, university example, Windows/WSL guide, architecture and format documentation.

Limits: one connection; in-memory rebuilt index nodes; no native Windows backend, JOIN/aggregation/MVCC/server, page recycling, or broad ACID certification. Unverified tools/platforms are recorded in the validation report.
