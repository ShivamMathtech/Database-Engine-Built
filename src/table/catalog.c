#include "cdb/internal.h"
#include "cdb/page.h"
#include <string.h>
#define SCHEMA_BASE 44u
#define SCHEMA_COLUMN 66u
CdbTable *cdb_catalog_find(Cdb *db, const char *name) {
    for (size_t i = 0; i < db->table_count; i++)
        if (strcmp(db->tables[i].name, name) == 0)
            return &db->tables[i];
    return NULL;
}
void cdb_catalog_free(Cdb *db) {
    for (size_t i = 0; i < db->table_count; i++)
        for (uint16_t j = 0; j < db->tables[i].column_count; j++)
            cdb_btree_free(&db->tables[i].indexes[j]);
    memset(db->tables, 0, sizeof(db->tables));
    db->table_count = 0;
}
static uint16_t encode(const CdbTable *t, uint8_t *out) {
    size_t n = SCHEMA_BASE + SCHEMA_COLUMN * t->column_count;
    memset(out, 0, n);
    cdb_put32(out, t->id);
    cdb_put32(out + 4, t->first_page);
    cdb_put16(out + 8, t->column_count);
    memcpy(out + 12, t->name, 32);
    for (uint16_t i = 0; i < t->column_count; i++) {
        const CdbColumn *c = &t->columns[i];
        uint8_t *p = out + SCHEMA_BASE + SCHEMA_COLUMN * i;
        p[0] = (uint8_t)c->type;
        p[1] = (uint8_t)((c->not_null ? 1u : 0u) | (c->primary ? 2u : 0u));
        memcpy(p + 2, c->name, 32);
        memcpy(p + 34, c->index_name, 32);
    }
    return (uint16_t)n;
}
static bool decode(CdbTable *t, const uint8_t *data, size_t n, CdbError *e) {
    memset(t, 0, sizeof(*t));
    if (n < SCHEMA_BASE)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Short catalog record");
    t->id = cdb_get32(data);
    t->first_page = cdb_get32(data + 4);
    t->last_page = t->first_page;
    t->column_count = cdb_get16(data + 8);
    if (!t->column_count || t->column_count > CDB_MAX_COLUMNS ||
        n != SCHEMA_BASE + SCHEMA_COLUMN * t->column_count || !memchr(data + 12, 0, 32))
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid catalog record");
    memcpy(t->name, data + 12, 32);
    if (!cdb_identifier(t->name))
        return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid table name in catalog");
    unsigned primary = 0;
    for (uint16_t i = 0; i < t->column_count; i++) {
        const uint8_t *p = data + SCHEMA_BASE + SCHEMA_COLUMN * i;
        CdbColumn *c = &t->columns[i];
        if (p[0] < CDB_INTEGER || p[0] > CDB_BOOLEAN || p[1] > 3 || !memchr(p + 2, 0, 32) ||
            !memchr(p + 34, 0, 32))
            return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid column metadata");
        c->type = (CdbType)p[0];
        c->not_null = (p[1] & 1u) != 0;
        c->primary = (p[1] & 2u) != 0;
        memcpy(c->name, p + 2, 32);
        memcpy(c->index_name, p + 34, 32);
        if (!cdb_identifier(c->name) || (c->index_name[0] && !cdb_identifier(c->index_name)) ||
            ((c->primary || c->index_name[0]) && c->type != CDB_INTEGER) ||
            (c->primary && !c->not_null))
            return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid indexed column definition");
        primary += c->primary ? 1u : 0u;
        for (uint16_t j = 0; j < i; j++)
            if (strcmp(c->name, t->columns[j].name) == 0)
                return cdb_fail(e, CDB_ERR_CORRUPT, "Duplicate column in catalog");
    }
    return primary <= 1 || cdb_fail(e, CDB_ERR_CORRUPT, "Multiple primary keys in catalog");
}
bool cdb_catalog_load(Cdb *db, CdbError *e) {
    cdb_catalog_free(db);
    if (db->page_count < 2)
        return cdb_fail(e, CDB_ERR_CORRUPT, "Missing catalog page");
    uint32_t id = 1;
    while (id != CDB_NO_PAGE) {
        uint8_t page[CDB_PAGE_SIZE];
        CDB_TRY(cdb_storage_read(db, id, page, e));
        if (page[0] != CDB_PAGE_CATALOG || cdb_get32(page + 12) != 0)
            return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid catalog page type/owner");
        for (uint16_t slot = 0; slot < cdb_page_slots(page); slot++) {
            const uint8_t *data;
            uint16_t n;
            CDB_TRY(cdb_page_read(page, slot, &data, &n, e));
            if (!n)
                continue;
            if (db->table_count == CDB_MAX_TABLES)
                return cdb_fail(e, CDB_ERR_CORRUPT, "Catalog table limit exceeded");
            CdbTable *t = &db->tables[db->table_count];
            CDB_TRY(decode(t, data, n, e));
            if (t->id != t->first_page || t->first_page < 2 || t->first_page >= db->page_count)
                return cdb_fail(e, CDB_ERR_CORRUPT, "Invalid table root");
            if (cdb_catalog_find(db, t->name))
                return cdb_fail(e, CDB_ERR_CORRUPT, "Duplicate table name");
            for (size_t j = 0; j < db->table_count; j++)
                if (db->tables[j].id == t->id)
                    return cdb_fail(e, CDB_ERR_CORRUPT, "Duplicate table id");
            t->catalog_rid = cdb_rid(id, slot);
            db->table_count++;
        }
        uint32_t next = cdb_page_next(page);
        if (next != CDB_NO_PAGE && (next <= id || next >= db->page_count))
            return cdb_fail(e, CDB_ERR_CORRUPT, "Cyclic/invalid catalog page link");
        id = next;
    }
    for (size_t a = 0; a < db->table_count; a++)
        for (uint16_t b = 0; b < db->tables[a].column_count; b++) {
            const char *name = db->tables[a].columns[b].index_name;
            if (!*name)
                continue;
            for (size_t c = 0; c <= a; c++)
                for (uint16_t d = 0; d < db->tables[c].column_count; d++) {
                    if (c == a && d >= b)
                        break;
                    if (strcmp(name, db->tables[c].columns[d].index_name) == 0)
                        return cdb_fail(e, CDB_ERR_CORRUPT, "Duplicate index name");
                }
        }
    return true;
}
bool cdb_catalog_save(Cdb *db, CdbTable *t, CdbError *e) {
    uint8_t data[SCHEMA_BASE + SCHEMA_COLUMN * CDB_MAX_COLUMNS];
    uint16_t n = encode(t, data);
    uint8_t *page;
    CDB_TRY(cdb_storage_edit(db, cdb_rid_page(t->catalog_rid), &page, e));
    return cdb_page_update(page, cdb_rid_slot(t->catalog_rid), data, n, e);
}
bool cdb_catalog_create(Cdb *db, const char *name, const CdbColumn *cols, uint16_t count,
                        CdbError *e) {
    if (!cdb_identifier(name) || !count || count > CDB_MAX_COLUMNS)
        return cdb_fail(e, CDB_ERR_CONSTRAINT, "Invalid table definition");
    if (cdb_catalog_find(db, name))
        return cdb_fail(e, CDB_ERR_EXISTS, "Table '%s' already exists", name);
    if (db->table_count == CDB_MAX_TABLES)
        return cdb_fail(e, CDB_ERR_LIMIT, "Maximum %u tables", CDB_MAX_TABLES);
    unsigned primary = 0;
    for (uint16_t i = 0; i < count; i++) {
        if (!cdb_identifier(cols[i].name) || cols[i].type < CDB_INTEGER ||
            cols[i].type > CDB_BOOLEAN)
            return cdb_fail(e, CDB_ERR_CONSTRAINT, "Invalid column definition");
        if (cols[i].primary) {
            primary++;
            if (cols[i].type != CDB_INTEGER)
                return cdb_fail(e, CDB_ERR_UNSUPPORTED, "PRIMARY KEY requires INTEGER in v0.1.0");
        }
        for (uint16_t j = 0; j < i; j++)
            if (strcmp(cols[i].name, cols[j].name) == 0)
                return cdb_fail(e, CDB_ERR_CONSTRAINT, "Duplicate column '%s'", cols[i].name);
    }
    if (primary > 1)
        return cdb_fail(e, CDB_ERR_UNSUPPORTED, "Only one primary key column is supported");
    CdbTable table = {0};
    memcpy(table.name, name, strlen(name) + 1);
    table.column_count = count;
    memcpy(table.columns, cols, count * sizeof(*cols));
    CDB_TRY(cdb_storage_allocate(db, CDB_PAGE_DATA, 0, &table.first_page, e));
    table.id = table.first_page;
    table.last_page = table.first_page;
    uint8_t *root;
    CDB_TRY(cdb_storage_edit(db, table.first_page, &root, e));
    cdb_put32(root + 12, table.id);
    uint8_t data[SCHEMA_BASE + SCHEMA_COLUMN * CDB_MAX_COLUMNS];
    uint16_t n = encode(&table, data);
    uint32_t id = 1;
    for (;;) {
        uint8_t *page;
        CDB_TRY(cdb_storage_edit(db, id, &page, e));
        uint16_t slot;
        CdbError local = {0};
        if (cdb_page_insert(page, data, n, &slot, &local)) {
            table.catalog_rid = cdb_rid(id, slot);
            break;
        }
        if (local.code != CDB_ERR_LIMIT) {
            if (e)
                *e = local;
            return false;
        }
        uint32_t next = cdb_page_next(page);
        if (next == CDB_NO_PAGE) {
            CDB_TRY(cdb_storage_allocate(db, CDB_PAGE_CATALOG, 0, &next, e));
            cdb_page_set_next(page, next);
        }
        id = next;
    }
    db->tables[db->table_count++] = table;
    return true;
}
bool cdb_catalog_drop(Cdb *db, const char *name, CdbError *e) {
    CdbTable *t = cdb_catalog_find(db, name);
    if (!t)
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Table '%s' does not exist", name);
    uint8_t *page;
    CDB_TRY(cdb_storage_edit(db, cdb_rid_page(t->catalog_rid), &page, e));
    CDB_TRY(cdb_page_delete(page, cdb_rid_slot(t->catalog_rid), e));
    size_t index = (size_t)(t - db->tables);
    for (uint16_t j = 0; j < t->column_count; j++)
        cdb_btree_free(&t->indexes[j]);
    for (size_t i = index + 1; i < db->table_count; i++)
        db->tables[i - 1] = db->tables[i];
    db->table_count--;
    memset(&db->tables[db->table_count], 0, sizeof(*t));
    return true;
}
