#include "cdb/executor.h"
#include "cdb/internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool numeric(CdbType t) {
    return t == CDB_INTEGER || t == CDB_FLOAT || t == CDB_NULL;
}
static bool truth_type(CdbType t) {
    return t == CDB_BOOLEAN || t == CDB_NULL;
}
static bool bind_expr(CdbExpr *x, const CdbTable *t, CdbType *type, CdbError *e) {
    if (!x) {
        *type = CDB_BOOLEAN;
        return true;
    }
    if (x->kind == EX_VALUE) {
        *type = x->value.type;
        return true;
    }
    if (x->kind == EX_COLUMN) {
        x->binding = cdb_table_column(t, x->column);
        if (x->binding < 0)
            return cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown column '%s'", x->column);
        *type = t->columns[x->binding].type;
        return true;
    }
    CdbType a, b = CDB_NULL;
    CDB_TRY(bind_expr(x->left, t, &a, e));
    if (x->kind == EX_UNARY) {
        if (x->binding == 1) {
            *type = CDB_BOOLEAN;
            return true;
        }
        if (x->op == TK_NOT) {
            if (!truth_type(a))
                return cdb_fail(e, CDB_ERR_TYPE, "NOT requires BOOLEAN");
            *type = CDB_BOOLEAN;
            return true;
        }
        if (!numeric(a))
            return cdb_fail(e, CDB_ERR_TYPE, "Unary sign requires a number");
        *type = a;
        return true;
    }
    CDB_TRY(bind_expr(x->right, t, &b, e));
    if (x->op == TK_AND || x->op == TK_OR) {
        if (!truth_type(a) || !truth_type(b))
            return cdb_fail(e, CDB_ERR_TYPE, "AND/OR require BOOLEAN expressions");
        *type = CDB_BOOLEAN;
        return true;
    }
    if (x->op == TK_PLUS || x->op == TK_MINUS || x->op == TK_STAR || x->op == TK_SLASH) {
        if (!numeric(a) || !numeric(b))
            return cdb_fail(e, CDB_ERR_TYPE, "Arithmetic requires numeric operands");
        *type = (a == CDB_FLOAT || b == CDB_FLOAT) ? CDB_FLOAT : CDB_INTEGER;
        return true;
    }
    if (a != CDB_NULL && b != CDB_NULL && a != b && !(numeric(a) && numeric(b)))
        return cdb_fail(e, CDB_ERR_TYPE, "Incompatible comparison types");
    *type = CDB_BOOLEAN;
    return true;
}
static bool int_arithmetic(CdbTokenType op, int64_t a, int64_t b, CdbValue *out, CdbError *e) {
    bool overflow = false;
    int64_t n = 0;
    switch (op) {
    case TK_PLUS:
        overflow = (b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b);
        if (!overflow)
            n = a + b;
        break;
    case TK_MINUS:
        overflow = (b < 0 && a > INT64_MAX + b) || (b > 0 && a < INT64_MIN + b);
        if (!overflow)
            n = a - b;
        break;
    case TK_STAR:
        if (a > 0)
            overflow = b > 0 ? a > INT64_MAX / b : (b < 0 && b < INT64_MIN / a);
        else if (a < 0)
            overflow = b > 0 ? a < INT64_MIN / b : (b < 0 && a < INT64_MAX / b);
        if (!overflow)
            n = a * b;
        break;
    case TK_SLASH:
        if (!b)
            return cdb_fail(e, CDB_ERR_TYPE, "Division by zero");
        overflow = a == INT64_MIN && b == -1;
        if (!overflow)
            n = a / b;
        break;
    default:
        return cdb_fail(e, CDB_ERR_INTERNAL, "Invalid arithmetic operator");
    }
    if (overflow)
        return cdb_fail(e, CDB_ERR_TYPE, "INTEGER arithmetic overflow");
    *out = cdb_integer(n);
    return true;
}
static bool evaluate(const CdbExpr *x, const CdbRow *row, CdbValue *out, CdbError *e) {
    *out = cdb_null();
    if (!x) {
        *out = cdb_boolean(true);
        return true;
    }
    if (x->kind == EX_VALUE)
        return cdb_value_copy(out, &x->value, e);
    if (x->kind == EX_COLUMN)
        return cdb_value_copy(out, &row->values[x->binding], e);
    CdbValue a = cdb_null(), b = cdb_null();
    CDB_TRY(evaluate(x->left, row, &a, e));
    bool ok = true;
    if (x->kind == EX_UNARY) {
        if (x->binding == 1)
            *out = cdb_boolean(x->op == TK_IS ? a.type == CDB_NULL : a.type != CDB_NULL);
        else if (a.type == CDB_NULL)
            *out = cdb_null();
        else if (x->op == TK_NOT)
            *out = cdb_boolean(!a.as.boolean);
        else if (x->op == TK_PLUS)
            ok = cdb_value_copy(out, &a, e);
        else if (a.type == CDB_INTEGER) {
            if (a.as.integer == INT64_MIN)
                ok = cdb_fail(e, CDB_ERR_TYPE, "INTEGER negation overflow");
            else
                *out = cdb_integer(-a.as.integer);
        } else
            *out = cdb_float(-a.as.real);
        cdb_value_free(&a);
        return ok;
    }
    if (!evaluate(x->right, row, &b, e)) {
        cdb_value_free(&a);
        return false;
    }
    if (x->op == TK_AND || x->op == TK_OR) {
        bool a_null = a.type == CDB_NULL, b_null = b.type == CDB_NULL;
        if (x->op == TK_AND) {
            if ((!a_null && !a.as.boolean) || (!b_null && !b.as.boolean))
                *out = cdb_boolean(false);
            else if (a_null || b_null)
                *out = cdb_null();
            else
                *out = cdb_boolean(true);
        } else {
            if ((!a_null && a.as.boolean) || (!b_null && b.as.boolean))
                *out = cdb_boolean(true);
            else if (a_null || b_null)
                *out = cdb_null();
            else
                *out = cdb_boolean(false);
        }
    } else if (a.type == CDB_NULL || b.type == CDB_NULL)
        *out = cdb_null();
    else if (x->op == TK_PLUS || x->op == TK_MINUS || x->op == TK_STAR || x->op == TK_SLASH) {
        if (a.type == CDB_INTEGER && b.type == CDB_INTEGER)
            ok = int_arithmetic(x->op, a.as.integer, b.as.integer, out, e);
        else {
            double av = a.type == CDB_FLOAT ? a.as.real : (double)a.as.integer,
                   bv = b.type == CDB_FLOAT ? b.as.real : (double)b.as.integer, n = 0;
            if (x->op == TK_SLASH && bv == 0)
                ok = cdb_fail(e, CDB_ERR_TYPE, "Division by zero");
            else {
                switch (x->op) {
                case TK_PLUS:
                    n = av + bv;
                    break;
                case TK_MINUS:
                    n = av - bv;
                    break;
                case TK_STAR:
                    n = av * bv;
                    break;
                default:
                    n = av / bv;
                    break;
                }
                if (!isfinite(n))
                    ok = cdb_fail(e, CDB_ERR_TYPE, "Non-finite arithmetic result");
                else
                    *out = cdb_float(n);
            }
        }
    } else {
        int cmp = 0;
        ok = cdb_value_compare(&a, &b, &cmp, e);
        bool yes = false;
        if (ok) {
            switch (x->op) {
            case TK_EQ:
                yes = cmp == 0;
                break;
            case TK_NE:
                yes = cmp != 0;
                break;
            case TK_LT:
                yes = cmp < 0;
                break;
            case TK_LE:
                yes = cmp <= 0;
                break;
            case TK_GT:
                yes = cmp > 0;
                break;
            case TK_GE:
                yes = cmp >= 0;
                break;
            default:
                ok = cdb_fail(e, CDB_ERR_INTERNAL, "Invalid comparison");
                break;
            }
            *out = cdb_boolean(yes);
        }
    }
    cdb_value_free(&a);
    cdb_value_free(&b);
    return ok;
}
static int choose_index(const CdbExpr *x, const CdbTable *t, int64_t *key) {
    if (!x)
        return -1;
    if (x->kind == EX_BINARY && x->op == TK_AND) {
        int c = choose_index(x->left, t, key);
        return c >= 0 ? c : choose_index(x->right, t, key);
    }
    if (x->kind != EX_BINARY || x->op != TK_EQ)
        return -1;
    const CdbExpr *col = x->left, *val = x->right;
    if (col->kind != EX_COLUMN) {
        col = x->right;
        val = x->left;
    }
    if (col->kind != EX_COLUMN || val->kind != EX_VALUE || val->value.type != CDB_INTEGER)
        return -1;
    int i = col->binding;
    if (t->columns[i].type != CDB_INTEGER ||
        !(t->columns[i].primary || t->columns[i].index_name[0]))
        return -1;
    *key = val->value.as.integer;
    return i;
}
static bool result_row(CdbResult *r, const CdbRow *row, CdbError *e) {
    if (r->row_count == CDB_MAX_RESULT_ROWS)
        return cdb_fail(e, CDB_ERR_LIMIT, "Result exceeds %u rows; use LIMIT", CDB_MAX_RESULT_ROWS);
    if (r->row_count == r->row_capacity) {
        size_t cap = r->row_capacity ? r->row_capacity * 2 : 16;
        if (cap > CDB_MAX_RESULT_ROWS)
            cap = CDB_MAX_RESULT_ROWS;
        CdbRow *rows = realloc(r->rows, cap * sizeof(*rows));
        if (!rows)
            return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate results");
        r->rows = rows;
        r->row_capacity = cap;
    }
    CDB_TRY(cdb_row_copy(&r->rows[r->row_count], row, e));
    r->row_count++;
    return true;
}
typedef struct {
    Cdb *db;
    CdbTable *table;
    CdbQuery *query;
    CdbResult *result;
    int projection[CDB_MAX_COLUMNS];
    CdbRid *rids;
    size_t count, capacity;
} Selection;
static bool select_row(Cdb *db, CdbTable *table, CdbRid rid, const CdbRow *row, void *context,
                       CdbError *e) {
    (void)db;
    (void)table;
    Selection *s = context;
    CdbQuery *q = s->query;
    s->result->examined++;
    if (q->kind == Q_SELECT && s->result->row_count >= q->limit)
        return true;
    CdbValue match;
    CDB_TRY(evaluate(q->where, row, &match, e));
    bool yes = match.type == CDB_BOOLEAN && match.as.boolean;
    cdb_value_free(&match);
    if (!yes)
        return true;
    if (q->kind == Q_SELECT) {
        CdbRow projected = {.count = s->result->column_count};
        /* Borrow temporarily; result_row deep-copies these values. */ for (uint16_t i = 0;
                                                                            i < projected.count;
                                                                            i++)
            projected.values[i] = row->values[s->projection[i]];
        return result_row(s->result, &projected, e);
    }
    if (s->count == 1000000u)
        return cdb_fail(e, CDB_ERR_LIMIT, "Mutation exceeds one million candidate rows");
    if (s->count == s->capacity) {
        size_t cap = s->capacity ? s->capacity * 2 : 128;
        CdbRid *ids = realloc(s->rids, cap * sizeof(*ids));
        if (!ids)
            return cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate mutation candidates");
        s->rids = ids;
        s->capacity = cap;
    }
    s->rids[s->count++] = rid;
    return true;
}
static bool index_row(CdbRid rid, void *context, CdbError *e) {
    Selection *s = context;
    CdbRow row;
    CDB_TRY(cdb_table_read(s->db, s->table, rid, &row, e));
    bool ok = select_row(s->db, s->table, rid, &row, context, e);
    cdb_row_free(&row);
    return ok;
}
static bool insert_rows(Cdb *db, CdbTable *t, CdbQuery *q, CdbResult *r, CdbError *e) {
    int mapping[CDB_MAX_COLUMNS];
    uint16_t count = q->name_count ? q->name_count : t->column_count;
    for (uint16_t i = 0; i < count; i++) {
        mapping[i] = q->name_count ? cdb_table_column(t, q->names[i]) : (int)i;
        if (mapping[i] < 0)
            return cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown column '%s'", q->names[i]);
        for (uint16_t j = 0; j < i; j++)
            if (mapping[i] == mapping[j])
                return cdb_fail(e, CDB_ERR_CONSTRAINT, "Duplicate INSERT column");
    }
    for (size_t i = 0; i < q->row_count; i++) {
        if (q->rows[i].count != count)
            return cdb_fail(e, CDB_ERR_CONSTRAINT, "INSERT column/value count mismatch");
        CdbRow row = {.count = t->column_count};
        bool ok = true;
        for (uint16_t j = 0; j < count && ok; j++)
            ok = cdb_value_copy(&row.values[mapping[j]], &q->rows[i].values[j], e);
        CdbRid rid;
        if (ok)
            ok = cdb_table_insert(db, t, &row, &rid, e);
        cdb_row_free(&row);
        if (!ok)
            return false;
        r->affected++;
    }
    return true;
}
static bool row_query(Cdb *db, CdbTable *t, CdbQuery *q, CdbResult *r, CdbError *e) {
    CdbType type;
    CDB_TRY(bind_expr(q->where, t, &type, e));
    if (!truth_type(type))
        return cdb_fail(e, CDB_ERR_TYPE, "WHERE requires a BOOLEAN expression");
    Selection s = {.db = db, .table = t, .query = q, .result = r};
    if (q->kind == Q_SELECT) {
        r->column_count = q->star ? t->column_count : q->name_count;
        for (uint16_t i = 0; i < r->column_count; i++) {
            int col = q->star ? (int)i : cdb_table_column(t, q->names[i]);
            if (col < 0)
                return cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown column '%s'", q->names[i]);
            s.projection[i] = col;
            memcpy(r->columns[i], t->columns[col].name, 32);
        }
    }
    for (uint16_t i = 0; i < q->assignment_count; i++) {
        CdbAssignment *a = &q->assignments[i];
        a->binding = cdb_table_column(t, a->column);
        if (a->binding < 0)
            return cdb_fail(e, CDB_ERR_NOT_FOUND, "Unknown column '%s'", a->column);
        for (uint16_t j = 0; j < i; j++)
            if (q->assignments[j].binding == a->binding)
                return cdb_fail(e, CDB_ERR_CONSTRAINT, "Duplicate SET column");
        CDB_TRY(bind_expr(a->expr, t, &type, e));
        CdbType target = t->columns[a->binding].type;
        if (type != CDB_NULL && type != target && !(target == CDB_FLOAT && type == CDB_INTEGER))
            return cdb_fail(e, CDB_ERR_TYPE, "SET expression type mismatch for '%s'", a->column);
    }
    int64_t key = 0;
    int index = choose_index(q->where, t, &key);
    if (index >= 0)
        snprintf(r->plan, sizeof(r->plan), "INDEX LOOKUP %s.%s (%s)", t->name,
                 t->columns[index].name,
                 t->columns[index].primary ? "PRIMARY KEY" : t->columns[index].index_name);
    else
        snprintf(r->plan, sizeof(r->plan), "FULL TABLE SCAN %s", t->name);
    cdb_log(db, 3, "%s", r->plan);
    if (q->explain) {
        r->column_count = 1;
        memcpy(r->columns[0], "plan", 5);
        CdbRow row = {.count = 1};
        CDB_TRY(cdb_text(&row.values[0], r->plan, strlen(r->plan), e));
        bool ok = result_row(r, &row, e);
        cdb_row_free(&row);
        return ok;
    }
    bool ok = index >= 0 ? cdb_btree_equal(&t->indexes[index], key, index_row, &s, e)
                         : cdb_table_scan(db, t, select_row, &s, e);
    for (size_t i = 0; i < s.count && ok; i++) {
        if (q->kind == Q_DELETE)
            ok = cdb_table_delete(db, t, s.rids[i], e);
        else {
            CdbRow old = {0}, updated = {0};
            ok = cdb_table_read(db, t, s.rids[i], &old, e) && cdb_row_copy(&updated, &old, e);
            for (uint16_t j = 0; j < q->assignment_count && ok; j++) {
                CdbAssignment *a = &q->assignments[j];
                CdbValue v;
                ok = evaluate(a->expr, &old, &v, e);
                if (ok) {
                    cdb_value_free(&updated.values[a->binding]);
                    updated.values[a->binding] = v;
                }
            }
            if (ok)
                ok = cdb_table_update(db, t, s.rids[i], &updated, e);
            cdb_row_free(&old);
            cdb_row_free(&updated);
        }
        if (ok)
            r->affected++;
    }
    free(s.rids);
    return ok;
}
bool cdb_executor_run(Cdb *db, CdbQuery *q, CdbResult *r, CdbError *e) {
    switch (q->kind) {
    case Q_BEGIN:
        return cdb_transaction_begin(db, e);
    case Q_COMMIT:
        return cdb_transaction_commit(db, e);
    case Q_ROLLBACK:
        return cdb_transaction_rollback(db, e);
    case Q_CREATE:
        return cdb_catalog_create(db, q->table, q->columns, q->column_count, e);
    case Q_DROP:
        return cdb_catalog_drop(db, q->table, e);
    default:
        break;
    }
    CdbTable *t = cdb_catalog_find(db, q->table);
    if (!t)
        return cdb_fail(e, CDB_ERR_NOT_FOUND, "Table '%s' does not exist", q->table);
    if (q->kind == Q_INDEX)
        return cdb_index_create(db, t, q->index_name, q->names[0], e);
    if (q->kind == Q_INSERT)
        return insert_rows(db, t, q, r, e);
    return row_query(db, t, q, r, e);
}
