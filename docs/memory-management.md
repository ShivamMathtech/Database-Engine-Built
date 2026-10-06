# Memory ownership and error paths

## Ownership rules

| Type | Owns | Release |
|---|---|---|
| `CdbValue` TEXT | `length+1` heap bytes | `cdb_value_free` |
| `CdbRow` | Its values' strings | `cdb_row_free` |
| `CdbExpr` | Children and literal value | `cdb_expr_free` |
| `CdbQuery` | Predicate, assignments, INSERT rows | `cdb_query_free` |
| `CdbResult` | Result array and deep-copied rows | `cdb_result_free` |
| `CdbBtree` | All nodes | `cdb_btree_free` |
| `CdbTransaction` | Page map and individually allocated after-images | discard/commit/rollback |
| `Cdb` | Pager, cache, schemas, indexes, transaction | `cdb_close` |

Lexer tokens borrow slices from SQL. The caller must keep SQL alive throughout `cdb_execute`, which parses/executes synchronously. AST strings are independent copies. Stored rows and returned results do not borrow SQL.

Copying a struct containing TEXT by ordinary assignment aliases its string pointer. Use `cdb_value_copy` / `cdb_row_copy` when ownership must be independent. The executor uses short-lived borrowed projections, immediately deep-copies them into results, and does not destroy those borrowed projections. Assignment evaluation produces owned temporaries and replaces values only after freeing the old destination.

## Page lifetimes

A pinned frame is borrowed from the committed cache until unpin. Storage reads copy the frame, then unpin, so callers do not retain evictable memory. Transaction after-images are allocated individually; growing the pointer map does not move page data. A pointer from `cdb_storage_edit` remains valid until transaction end. Never keep it across commit/rollback/close.

This distinction matters when allocating a new page while holding an edited previous page to update its next pointer. A single reallocating vector of page structs would invalidate that pointer; the pointer-map design avoids that bug.

## Failure behavior

All fallible constructors return false, clean up partial owned state where necessary, and set a bounded error. AST cleanup is safe after partial parsing. Row deserialization initializes the output before validating, frees partial strings on failure, and checks bounds before using lengths. Arithmetic checks precede signed operations that could overflow. Page updates use a scratch copy for atomic failure.

SQL mutation errors roll back the containing transaction to remove partial index/storage changes. Commit I/O errors are different: their durability outcome can be uncertain, so the connection becomes unusable until reopened. Closing frees memory without deleting a potentially recoverable WAL.

## Checks and remaining limits

Run `make test-asan`, `make test-ubsan`, and `make valgrind` where supported. Release tests use explicit CHECK macros, not `assert`, so optimization/NDEBUG cannot erase test behavior. Allocation-failure injection and exhaustive OOM-path testing are **NOT IMPLEMENTED** and are on the roadmap.

No test can prove absence of every memory defect. The supplied report distinguishes executed address/undefined-behavior checks from leak detection that was blocked by this environment. Bounded input/result/transaction sizes reduce resource exhaustion but this is not an adversarial network service.
