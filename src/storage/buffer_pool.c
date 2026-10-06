#include "cdb/buffer_pool.h"
#include <string.h>
void cdb_buffer_init(CdbBufferPool *b, CdbPager *p) {
    memset(b, 0, sizeof(*b));
    b->pager = p;
}
bool cdb_buffer_pin(CdbBufferPool *b, uint32_t id, CdbFrame **out, CdbError *e) {
    for (size_t i = 0; i < CDB_CACHE_FRAMES; i++) {
        CdbFrame *f = &b->frames[i];
        if (f->valid && f->id == id) {
            f->pins++;
            f->referenced = true;
            b->hits++;
            *out = f;
            return true;
        }
    }
    b->misses++;
    for (size_t i = 0; i < 2u * CDB_CACHE_FRAMES; i++) {
        CdbFrame *f = &b->frames[b->hand];
        b->hand = (b->hand + 1) % CDB_CACHE_FRAMES;
        if (f->pins)
            continue;
        if (f->valid && f->referenced) {
            f->referenced = false;
            continue;
        }
        if (f->valid)
            b->evictions++;
        f->valid = false;
        CDB_TRY(cdb_pager_read(b->pager, id, f->data, e));
        f->id = id;
        f->valid = true;
        f->referenced = true;
        f->pins = 1;
        *out = f;
        return true;
    }
    return cdb_fail(e, CDB_ERR_BUSY, "All buffer frames are pinned");
}
void cdb_buffer_unpin(CdbFrame *f) {
    if (f->pins)
        f->pins--;
}
void cdb_buffer_invalidate(CdbBufferPool *b) {
    for (size_t i = 0; i < CDB_CACHE_FRAMES; i++)
        b->frames[i].valid = false;
}
