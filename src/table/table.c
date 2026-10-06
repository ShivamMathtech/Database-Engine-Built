#include "cdb/internal.h"
#include "cdb/page.h"
#include <math.h>
#include <string.h>
int cdb_table_column(const CdbTable *t, const char *name) {
    for (uint16_t i = 0; i < t->column_count; i++)
        if (strcmp(t->columns[i].name, name) == 0)
            return (int)i;
    return -1;
}
bool cdb_table_validate_row(const CdbTable *t, CdbRow *r, CdbError *e) {
    if (r->count != t->column_count)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Table '%s' expects %u columns, got %u", t->name,
                        (unsigned)t->column_count, (unsigned)r->count);
    for (uint16_t i = 0; i < r->count; i++) {
        CdbValue *v = &r->values[i];
        const CdbColumn *c = &t->columns[i];
        if (v->type == CDB_NULL) {
            if (c->not_null)
                return cdb_fail(e, CDB_ERR_CONSTRAINT, "Column '%s' cannot be NULL", c->name);
            continue;
        }
        if (c->type == CDB_FLOAT && v->type == CDB_INTEGER)
            *v = cdb_float((double)v->as.integer);
        if (v->type != c->type)
            return cdb_fail(e, CDB_ERR_TYPE, "Column '%s' requires %s; got %s", c->name,
                            cdb_type_name(c->type), cdb_type_name(v->type));
        if (v->type == CDB_FLOAT && !isfinite(v->as.real))
            return cdb_fail(e, CDB_ERR_TYPE, "Non-finite FLOAT");
    }
    return cdb_row_size(r) <= CDB_MAX_RECORD ||
           cdb_fail(e, CDB_ERR_LIMIT, "Row exceeds %u bytes", CDB_MAX_RECORD);
}
static bool data_page(CdbTable *t, const uint8_t *p, CdbError *e) {
    return (p[0] == CDB_PAGE_DATA && cdb_get32(p + 12) == t->id) ||
           cdb_fail(e, CDB_ERR_CORRUPT, "Page belongs to another table/type");
}
bool cdb_table_scan(Cdb *db, CdbTable *t, CdbRowVisitor visit, void *ctx, CdbError *e) {
    uint32_t id = t->first_page;
    while (id != CDB_NO_PAGE) {
        uint8_t page[CDB_PAGE_SIZE];
        CDB_TRY(cdb_storage_read(db, id, page, e));
        CDB_TRY(data_page(t, page, e));
        t->last_page = id;
        for (uint16_t slot = 0; slot < cdb_page_slots(page); slot++) {
            const uint8_t *bytes;
            uint16_t n;
            CDB_TRY(cdb_page_read(page, slot, &bytes, &n, e));
            if (!n)
                continue;
            CdbRow row;
            CDB_TRY(cdb_row_deserialize(&row, bytes, n, e));
            bool ok =
                cdb_table_validate_row(t, &row, e) && visit(db, t, cdb_rid(id, slot), &row, ctx, e);
            cdb_row_free(&row);
            if (!ok)
                return false;
        }
        uint32_t next = cdb_page_next(page);
        if (next != CDB_NO_PAGE && (next <= id || next >= db->page_count))
            return cdb_fail(e, CDB_ERR_CORRUPT, "Cyclic/invalid table page chain");
        id = next;
    }
    return true;
}
bool cdb_table_read(Cdb *db, CdbTable *t, CdbRid rid, CdbRow *row, CdbError *e) {
    memset(row, 0, sizeof(*row));
    uint8_t page[CDB_PAGE_SIZE];
    CDB_TRY(cdb_storage_read(db, cdb_rid_page(rid), page, e));
    CDB_TRY(data_page(t, page, e));
    const uint8_t *bytes;
    uint16_t n;
    CDB_TRY(cdb_page_read(page, cdb_rid_slot(rid), &bytes, &n, e));
    if (!n)
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Row is deleted");
    CDB_TRY(cdb_row_deserialize(row, bytes, n, e));
    if (!cdb_table_validate_row(t, row, e)) {
        cdb_row_free(row);
        return false;
    }
    return true;
}
bool cdb_table_insert(Cdb *db, CdbTable *t, CdbRow *row, CdbRid *rid, CdbError *e) {
    CDB_TRY(cdb_table_validate_row(t, row, e));
    CDB_TRY(cdb_index_check(t, row, UINT64_MAX, e));
    uint8_t data[CDB_MAX_RECORD];
    size_t n;
    CDB_TRY(cdb_row_serialize(row, data, sizeof(data), &n, e));
    uint8_t *page;
    CDB_TRY(cdb_storage_edit(db, t->last_page, &page, e));
    uint16_t slot;
    CdbError local = {0};
    if (!cdb_page_insert(page, data, (uint16_t)n, &slot, &local)) {
        if (local.code != CDB_ERR_LIMIT) {
            if (e)
                *e = local;
            return false;
        }
        uint32_t next;
        CDB_TRY(cdb_storage_allocate(db, CDB_PAGE_DATA, t->id, &next, e));
        cdb_page_set_next(page, next);
        t->last_page = next;
        CDB_TRY(cdb_storage_edit(db, next, &page, e));
        CDB_TRY(cdb_page_insert(page, data, (uint16_t)n, &slot, e));
    }
    *rid = cdb_rid(t->last_page, slot);
    return cdb_index_add(t, row, *rid, e);
}
bool cdb_table_delete(Cdb *db, CdbTable *t, CdbRid rid, CdbError *e) {
    CdbRow old;
    CDB_TRY(cdb_table_read(db, t, rid, &old, e));
    uint8_t *page;
    bool ok = cdb_storage_edit(db, cdb_rid_page(rid), &page, e) &&
              cdb_page_delete(page, cdb_rid_slot(rid), e);
    if (ok)
        cdb_index_remove(t, &old, rid);
    cdb_row_free(&old);
    return ok;
}
bool cdb_table_update(Cdb *db, CdbTable *t, CdbRid rid, CdbRow *row, CdbError *e) {
    CDB_TRY(cdb_table_validate_row(t, row, e));
    CDB_TRY(cdb_index_check(t, row, rid, e));
    uint8_t data[CDB_MAX_RECORD];
    size_t n;
    CDB_TRY(cdb_row_serialize(row, data, sizeof(data), &n, e));
    CdbRow old;
    CDB_TRY(cdb_table_read(db, t, rid, &old, e));
    uint8_t *page;
    if (!cdb_storage_edit(db, cdb_rid_page(rid), &page, e)) {
        cdb_row_free(&old);
        return false;
    }
    CdbError local = {0};
    bool ok = cdb_page_update(page, cdb_rid_slot(rid), data, (uint16_t)n, &local);
    if (ok) {
        cdb_index_remove(t, &old, rid);
        ok = cdb_index_add(t, row, rid, e);
    } else if (local.code == CDB_ERR_LIMIT) {
        cdb_index_remove(t, &old, rid);
        CdbRid relocated;
        ok = cdb_page_delete(page, cdb_rid_slot(rid), e) &&
             cdb_table_insert(db, t, row, &relocated, e);
    } else if (e)
        *e = local;
    cdb_row_free(&old);
    return ok;
}
