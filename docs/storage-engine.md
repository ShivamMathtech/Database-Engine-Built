# Storage engine and `.cdb` format (version 1)

## Byte conventions

Pages are exactly 4096 bytes. Page `id` begins at byte `id * 4096`. All unsigned integers use explicit little-endian encoding. Signed INTEGER values are stored as their modulo-2^64 representation and reconstructed without an out-of-range unsigned-to-signed cast. FLOAT uses the IEEE-754 binary64 bit pattern, encoded as a little-endian u64. Unsupported floating representations fail a compile-time assertion.

No persisted format contains a C pointer, `size_t`, struct padding, native endianness, or native enum width. Reserved fields are zero on writes. Offsets below are relative to the start of a page/record. The page size is a format constant, not a runtime tuning parameter.

## Page 0: superblock

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 4 | ASCII `CDB1` |
| 4 | 4 | Format version = 1 |
| 8 | 4 | Page size = 4096 |
| 12 | 4 | File page count, at most 1,048,576 |
| 16 | 8 | Last committed generation, starts at 1 |
| 24 | 4 | Catalog root = page 1 |
| 28 | 4 | Reserved |
| 32 | 16 | Random database identity from OS random device |
| 48 | 4044 | Reserved zeros |
| 4092 | 4 | FNV-1a checksum over bytes 0..4091 |

Open checks format, checksum, nonzero generation, catalog root, page count, and exact file length. An unsupported version fails cleanly. There is no migration utility yet.

## Catalog/data page header

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 1 | Type: 1=catalog, 2=table data |
| 1 | 1 | Reserved |
| 2 | 2 | Slot count (includes tombstones) |
| 4 | 2 | Reserved |
| 6 | 2 | Start of packed payload/free-area boundary |
| 8 | 4 | Next page ID; `0xffffffff` means end |
| 12 | 4 | Owner: 0=catalog, otherwise table ID |
| 16 | 4 | This page's ID |
| 20 | 4 | Reserved |
| 24 | 4 × slots | Slot directory |
| ... | variable | Free space between directory and payload |
| ... | variable | Records packed backward from byte 4092 |
| 4092 | 4 | Page checksum |

A slot is `(u16 record_offset, u16 record_length)`. A deleted slot is `(0,0)`. An empty row is never valid. The maximum encoded record is `4092 - 24 - 4 = 4064` bytes. `cdb_page_validate` checks page ID/type, directory bounds, offsets, lengths, tombstones, and payload overlap. Committed pages have their checksums checked by the pager; private after-images are resealed at commit.

`page_insert` first proves enough total free space exists, compacts, then inserts into a tombstone or a new slot. `page_update` operates on a scratch page and publishes only if it fits. `page_delete` tombstones a slot. Compaction changes byte offsets but preserves slot numbers.

## Record IDs and rows

A runtime record ID is `((uint64_t)page_id << 16) | slot_id`. It is an internal location, not a stable user key: a growing UPDATE may relocate the row. Index entries are updated when this happens.

A row begins with `u16 column_count`, then each value in schema order:

| Tag | Value | Following bytes |
|---:|---|---|
| 0 | NULL | none |
| 1 | INTEGER | 8-byte signed representation |
| 2 | FLOAT | 8-byte binary64 bits, finite only |
| 3 | TEXT | u32 byte length, then that many bytes, no NUL terminator |
| 4 | BOOLEAN | one byte, 0 or 1 |

The codec rejects unknown tags, truncated fields, non-finite floats, embedded NUL text, invalid boolean bytes, column overflow, and trailing bytes. Deserialized TEXT owns a newly allocated trailing-NUL string for safe C operations. A row's column count and types are checked against its table before query evaluation.

## Catalog records

| Offset | Bytes | Meaning |
|---:|---:|---|
| 0 | 4 | Table ID (equals first data page ID) |
| 4 | 4 | First data page |
| 8 | 2 | Column count, 1..16 |
| 10 | 2 | Reserved |
| 12 | 32 | Table identifier, NUL terminated |
| 44 | 66 × columns | Column records |

Each 66-byte column record contains type u8, flags u8 (`1=NOT NULL`, `2=PRIMARY KEY`), a 32-byte column name, and a 32-byte index name (empty means no secondary index). Primary keys have an implicit unique index even with an empty index name. Only INTEGER columns can be indexed. Names, primary-key count/type, duplicate table/column/index names, roots, and record sizes are validated.

## Pager and cache

The pager owns the database/WAL descriptors and exclusive advisory lock. Its positioned I/O loops handle interrupted and short operations, and treat unexpected EOF as corruption. The connection uses 64 clock-replacement cache frames. A hit sets a reference bit; eviction skips pinned frames and clears reference bits on a first pass. A request fails BUSY if every frame is pinned.

The committed cache is read-only. On first modification, `cdb_storage_edit` makes a private 4096-byte after-image. Later reads first consult the transaction page map. Thus cache eviction never flushes uncommitted changes. COMMIT flushes dirty pages only through the WAL protocol, then invalidates cached copies. This is the reason there is no unsafe public “flush dirty cache to disk” shortcut.

## Allocation and deletion limits

The file grows by page allocation. Tables insert at their tail, reusing tombstones there. UPDATE compacts in place or moves the row to the tail. DELETE leaves slots/pages reusable only where current algorithms inspect them. DROP tombstones its catalog row and frees its RAM indexes; its data pages remain orphaned. There are no overflow pages, vacuum, free-page recycling, or file shrinking in v0.1.0.

Page chains must increase monotonically, so a self-link, backward link, wrong owner/type, or out-of-range page is rejected. The 4 GiB format limit and 256 MiB dirty-payload limit are defensive limits, not verified workload sizes. The executed stress size is recorded separately.
