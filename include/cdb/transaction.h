#ifndef CDB_TRANSACTION_H
#define CDB_TRANSACTION_H
#include "table.h"
typedef struct {
    uint32_t id;
    uint8_t data[CDB_PAGE_SIZE];
} CdbChange;
typedef struct {
    bool active;
    uint32_t original_pages;
    CdbChange **map;
    size_t map_size, dirty_count;
} CdbTransaction;
bool cdb_transaction_begin(Cdb *db, CdbError *e);
bool cdb_transaction_commit(Cdb *db, CdbError *e);
bool cdb_transaction_rollback(Cdb *db, CdbError *e);
void cdb_transaction_discard(Cdb *db);
#endif
