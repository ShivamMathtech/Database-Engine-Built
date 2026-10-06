# Contributing to CDB

Build with `make`, run `make test`, and run the relevant stress/sanitizer targets before sending a change. Keep SQL parsing, query binding/evaluation, page storage, and presentation separate. New public functions need header declarations and explicit ownership/error contracts.

A useful pull request describes the concrete problem, resulting behavior, implementation choice, and tests that protect it. Add a minimal regression case for an observed bug. For a file-format change, specify versioning, migration/rejection behavior, crash points, and malformed-input checks before implementing it.

Use C17 and the existing POSIX backend. Avoid new dependencies unless the proposal explains why they are needed. Preserve `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes -Werror`. Do not silence a warning to hide an ownership or bounds problem. `make format` applies `.clang-format`; `make lint` runs GCC's analyzer.

Never include real private databases, credentials, generated objects, or fabricated test/benchmark results. Use deterministic seeds. Distinguish implemented, tested, unverified, and future work in documentation. The `docs/commit-roadmap.md` sequence is a guide to coherent changes, not a requirement to rewrite history.

When publishing the repository, maintainers should configure a private vulnerability-reporting channel before advertising it. Until then, discuss non-sensitive reproductions and coordinate a private contact without posting private data.
