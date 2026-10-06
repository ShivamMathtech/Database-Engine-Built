#include "cdb/internal.h"
#include <string.h>
bool cdb_index_check(const CdbTable *t, const CdbRow *row, CdbRid except, CdbError *e) {
    for (uint16_t i = 0; i < t->column_count; i++)
        if (t->columns[i].primary) {
            CdbRid found;
            if (cdb_btree_find(&t->indexes[i], row->values[i].as.integer, &found) &&
                found != except)
                return cdb_fail(e, CDB_ERR_CONSTRAINT, "Duplicate primary key in '%s.%s'", t->name,
                                t->columns[i].name);
        }
    return true;
}
bool cdb_index_add(CdbTable *t, const CdbRow *row, CdbRid rid, CdbError *e) {
    for (uint16_t i = 0; i < t->column_count; i++)
        if ((t->columns[i].primary || t->columns[i].index_name[0]) &&
            row->values[i].type != CDB_NULL)
            CDB_TRY(cdb_btree_insert(&t->indexes[i], row->values[i].as.integer, rid, e));
    return true;
}
void cdb_index_remove(CdbTable *t, const CdbRow *row, CdbRid rid) {
    for (uint16_t i = 0; i < t->column_count; i++)
        if ((t->columns[i].primary || t->columns[i].index_name[0]) &&
            row->values[i].type != CDB_NULL)
            (void)cdb_btree_delete(&t->indexes[i], row->values[i].as.integer, rid);
}
static bool rebuild_row(Cdb *db, CdbTable *t, CdbRid rid, const CdbRow *row, void *ctx,
                        CdbError *e) {
    (void)db;
    (void)ctx;
    CDB_TRY(cdb_index_check(t, row, UINT64_MAX, e));
    return cdb_index_add(t, row, rid, e);
}
bool cdb_index_rebuild(Cdb *db, CdbError *e) {
    for (size_t i = 0; i < db->table_count; i++) {
        CdbTable *t = &db->tables[i];
        for (uint16_t j = 0; j < t->column_count; j++)
            cdb_btree_free(&t->indexes[j]);
        CDB_TRY(cdb_table_scan(db, t, rebuild_row, NULL, e));
    }
    return true;
}
bool cdb_index_create(Cdb *db, CdbTable *t, const char *name, const char *column, CdbError *e) {
    int col = cdb_table_column(t, column);
    if (col < 0)
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown column '%s'", column);
    if (t->columns[col].type != CDB_INTEGER)
        return cdb_fail(e, CDB_ERR_UNSUPPORTED, "v0.1.0 indexes require INTEGER columns");
    if (t->columns[col].primary || t->columns[col].index_name[0])
        return cdb_fail(e, CDB_ERR_EXISTS, "Column already has an index");
    for (size_t i = 0; i < db->table_count; i++)
        for (uint16_t j = 0; j < db->tables[i].column_count; j++)
            if (strcmp(db->tables[i].columns[j].index_name, name) == 0)
                return cdb_fail(e, CDB_ERR_EXISTS, "Index '%s' already exists", name);
    memcpy(t->columns[col].index_name, name, strlen(name) + 1);
    CDB_TRY(cdb_catalog_save(db, t, e));
    /* Rebuild all trees once; ordinary row mutations subsequently maintain them incrementally. */
    return cdb_index_rebuild(db, e);
}
