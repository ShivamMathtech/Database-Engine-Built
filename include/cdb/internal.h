#ifndef CDB_INTERNAL_H
#define CDB_INTERNAL_H
/* Private connection definition. Public users only include cdb.h. */
#include "buffer_pool.h"
#include "catalog.h"
#include "cdb.h"
#include "index.h"
#include "storage.h"
#include "transaction.h"
struct Cdb {
    CdbPager pager;
    CdbBufferPool cache;
    CdbTransaction tx;
    uint32_t page_count;
    uint64_t generation;
    uint8_t identity[16];
    CdbTable tables[CDB_MAX_TABLES];
    size_t table_count;
    bool poisoned;
    int log_level;
};
void cdb_log(Cdb *db, int level, const char *fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
#endif
