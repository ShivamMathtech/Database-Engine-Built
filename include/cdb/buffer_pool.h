#ifndef CDB_BUFFER_POOL_H
#define CDB_BUFFER_POOL_H
#include "pager.h"
/* Committed pages only. Dirty pages live in the transaction write set (no-steal). */
typedef struct {
    uint32_t id, pins;
    bool valid, referenced;
    uint8_t data[CDB_PAGE_SIZE];
} CdbFrame;
typedef struct {
    CdbPager *pager;
    CdbFrame frames[CDB_CACHE_FRAMES];
    size_t hand;
    uint64_t hits, misses, evictions;
} CdbBufferPool;
void cdb_buffer_init(CdbBufferPool *b, CdbPager *p);
bool cdb_buffer_pin(CdbBufferPool *b, uint32_t id, CdbFrame **frame, CdbError *e);
void cdb_buffer_unpin(CdbFrame *frame);
void cdb_buffer_invalidate(CdbBufferPool *b);
#endif
