#ifndef CDB_TABLE_H
#define CDB_TABLE_H
#include "btree.h"
typedef struct Cdb Cdb;
typedef struct {
    char name[CDB_NAME_MAX + 1];
    CdbType type;
    bool not_null, primary;
    char index_name[CDB_NAME_MAX + 1];
} CdbColumn;
typedef struct {
    char name[CDB_NAME_MAX + 1];
    uint32_t id, first_page, last_page;
    CdbRid catalog_rid;
    uint16_t column_count;
    CdbColumn columns[CDB_MAX_COLUMNS];
    CdbBtree indexes[CDB_MAX_COLUMNS];
} CdbTable;
typedef bool (*CdbRowVisitor)(Cdb *db, CdbTable *t, CdbRid rid, const CdbRow *row, void *ctx,
                              CdbError *e);
int cdb_table_column(const CdbTable *t, const char *name);
bool cdb_table_validate_row(const CdbTable *t, CdbRow *row, CdbError *e);
bool cdb_table_scan(Cdb *db, CdbTable *t, CdbRowVisitor visit, void *ctx, CdbError *e);
bool cdb_table_read(Cdb *db, CdbTable *t, CdbRid rid, CdbRow *row, CdbError *e);
bool cdb_table_insert(Cdb *db, CdbTable *t, CdbRow *row, CdbRid *rid, CdbError *e);
bool cdb_table_delete(Cdb *db, CdbTable *t, CdbRid rid, CdbError *e);
bool cdb_table_update(Cdb *db, CdbTable *t, CdbRid rid, CdbRow *row, CdbError *e);
#endif
