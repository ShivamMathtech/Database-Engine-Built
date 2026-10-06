#include "cdb/page.h"
#include "test.h"
bool test_storage(void) {
    CdbError e = {0};
    uint8_t page[CDB_PAGE_SIZE];
    cdb_page_init(page, CDB_PAGE_DATA, 7, 3);
    uint16_t a, b;
    uint8_t text[3000];
    memset(text, 'x', sizeof(text));
    OK(cdb_page_insert(page, text, 1000, &a, &e));
    OK(cdb_page_insert(page, text, 2000, &b, &e));
    CHECK(a == 0 && b == 1);
    OK(cdb_page_delete(page, a, &e));
    OK(cdb_page_update(page, b, text, 3000, &e));
    cdb_page_compact(page);
    OK(cdb_page_validate(page, 7, &e));
    const uint8_t *data;
    uint16_t n;
    OK(cdb_page_read(page, b, &data, &n, &e));
    CHECK(n == 3000 && data[2999] == 'x');
    uint8_t original[CDB_PAGE_SIZE];
    memcpy(original, page, sizeof(page));
    CHECK(!cdb_page_insert(page, text, 2000, &a, &e));
    CHECK(memcmp(original, page, sizeof(page)) == 0);
    uint8_t corrupt[CDB_PAGE_SIZE];
    memcpy(corrupt, page, sizeof(corrupt));
    cdb_put16(corrupt + CDB_PAGE_HEADER, cdb_get16(corrupt + CDB_PAGE_HEADER + 4));
    cdb_put16(corrupt + CDB_PAGE_HEADER + 2, 100);
    CHECK(!cdb_page_validate(corrupt, 7, &e));
    cdb_seal(page);
    CHECK(cdb_checksum(page));
    page[100] ^= 1;
    CHECK(!cdb_checksum(page));
    cdb_put16(page + 2, UINT16_MAX);
    CHECK(!cdb_page_validate(page, 7, &e));
    Fixture f;
    CHECK(fixture_open(&f));
    OK(cdb_transaction_begin(f.db, &e));
    for (unsigned i = 0; i < 70; i++) {
        uint32_t id;
        OK(cdb_storage_allocate(f.db, CDB_PAGE_DATA, 0, &id, &e));
    }
    OK(cdb_transaction_commit(f.db, &e));
    CdbFrame *frames[CDB_CACHE_FRAMES];
    for (uint32_t i = 0; i < CDB_CACHE_FRAMES; i++)
        OK(cdb_buffer_pin(&f.db->cache, i, &frames[i], &e));
    CdbFrame *extra;
    CHECK(!cdb_buffer_pin(&f.db->cache, 65, &extra, &e));
    CHECK(e.code == CDB_ERR_BUSY);
    for (unsigned i = 0; i < CDB_CACHE_FRAMES; i++)
        cdb_buffer_unpin(frames[i]);
    OK(cdb_buffer_pin(&f.db->cache, 65, &extra, &e));
    cdb_buffer_unpin(extra);
    CHECK(f.db->cache.evictions > 0);
    CHECK(!cdb_storage_read(f.db, f.db->page_count, page, &e));
    fixture_close(&f);
    return true;
}
