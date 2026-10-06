# Security and reliability policy

CDB v0.1.0 is an educational embedded engine, not an internet-facing service or audited security product. Do not use it as the sole repository of critical or irreplaceable data. It has no encryption, authentication, SQL parameters, access control, or concurrency layer.

Input is bounded; disk lengths/counts/checksums/page links/types are validated. The backend uses a cooperating process lock and restrictive permissions for newly created files. These are engineering protections, not cryptographic authenticity or an operating-system sandbox. Hard-linked database/WAL files are rejected. Uncooperative writers, network-filesystem semantics, failing hardware, lost WAL files, and maliciously constructed checksummed data are outside the tested recovery model.

If a potential memory-safety or data-loss defect is found, preserve a copy of the database and WAL, capture a minimal non-sensitive reproduction, and contact maintainers privately using the channel configured by the published repository. No private reporting endpoint exists in this source ZIP. Do not publish private records or credentials while reporting a bug.

See `docs/transactions.md` for commit uncertainty and recovery behavior, and `docs/validation.md` for what was actually executed. There is no promised support SLA or claim that future releases are already supported.
