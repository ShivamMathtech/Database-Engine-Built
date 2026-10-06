# Twelve-phase implementation and audit guide

This document maps the requested phases to the implementation. Build/test evidence is in `validation.md`; reading a design alone is not evidence of a passing test.

| Phase | What / why / mechanism | Files and API | Verification and remaining limit |
|---|---|---|---|
| 1. Architecture | Separate SQL, storage, identity, and durability; define owners and invariants | `architecture.md`, `storage-engine.md`, `include/cdb/` | Interfaces compile together; single connection only |
| 2. Repository | Layered sources and independent tests keep responsibilities visible | `repository-tree.md`, Makefile paths | All source paths built; no scaffold TODO functions |
| 3. Core | Own strings, centralize errors, encode integers explicitly | `core/{error,types,value}.c`; copy/free/compare | Value/row tests, large integer comparison, sanitizer checks |
| 4. SQL | Tokens → AST with precedence, type binding later | lexer/parser/AST; `cdb_parse` | Grammar cases, malformed inputs, 10k deterministic random strings; SQL subset only |
| 5. Storage | Pack rows into slots, validate pages, cache committed reads | row/page/pager/cache/catalog/table/storage | Truncation, overlap/bounds, compaction, cache pinning, persistence; no overflow/free list |
| 6. B-Tree | Reduce lookup work using ordered composite integer/RID entries | `btree.c`; insert/find/equal/delete/validate/print | 12k keys, random deletion, duplicate secondary keys; nodes in RAM |
| 7. Query engine | Bind columns/types, choose equality index or scan, return owned rows | `executor.c`, `cdb_execute` | CRUD, NULL logic, arithmetic, plan choice, index maintenance; no JOIN/aggregate |
| 8. Transactions | Private after-images and redo before database writes | transaction/WAL; begin/commit/rollback | DDL/data rollback, five crash stages, corrupt WAL rejection; no MVCC/power-loss certification |
| 9. Tests | Deterministic assertions reveal regressions in each invariant | `tests/unit`, `integration`, `stress` | Debug/release, address/UB checks, 10k/100k data; leak enumeration unavailable here |
| 10. Performance | Time real operations, verify plans/results | `benchmarks/`; monotonic timing | Recorded measurements; no cross-engine or broad throughput claim |
| 11. Tooling | Repeatable warning gates, builds, static analysis, CI | Makefile/scripts/workflow/config | GCC executed locally; other runners/optional tools explicitly unverified |
| 12. Documentation | Explain mechanisms, setup, ownership, binary format, limitations | README and `docs/` | Commands audited, examples exercised, validation report included |

## Final-audit questions

- Does it compile and link? See the recorded strict GCC debug/release commands.
- Do tests cover core functionality? Eleven named groups plus CLI and stress workloads are provided.
- Does data persist? API reopen and independent CLI restart checks verify it.
- Does B-Tree integrity hold under tested operations? Occupancy/order/depth/count checks run through insertion/deletion workloads.
- Are ownership rules explicit? See `memory-management.md` and the public API tutorial.
- Do README and CI paths exist? Repository/package validation checks these paths and example commands.
- Are badges honest? Only static language/license/version/dependency badges are used; no invented build/stars badges.
- Are absent features labeled? Roadmap, platform table, and capability limits distinguish them from implemented features.
- Does simulated process recovery imply all failures are solved? No; transaction documentation states the tested failure model and unresolved verification work.
