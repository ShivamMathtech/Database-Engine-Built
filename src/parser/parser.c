#include "cdb/parser.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
    CdbLexer lexer;
    CdbToken token;
    CdbError *error;
    unsigned depth, nodes;
} Parser;
static bool advance(Parser *p) {
    return cdb_lexer_next(&p->lexer, &p->token, p->error);
}
static bool syntax(Parser *p, const char *what) {
    if (p->error)
        p->error->position = p->token.position;
    return cdb_fail(p->error, CDB_ERR_SYNTAX, "Expected %s at byte %zu", what, p->token.position);
}
static bool expect(Parser *p, CdbTokenType type, const char *what) {
    if (p->token.type != type)
        return syntax(p, what);
    return advance(p);
}
static bool name(Parser *p, char *dst) {
    if (p->token.type != TK_IDENT)
        return syntax(p, "identifier");
    if (p->token.length > CDB_NAME_MAX)
        return cdb_fail(p->error, CDB_ERR_LIMIT, "Identifier exceeds %u bytes", CDB_NAME_MAX);
    memcpy(dst, p->token.start, p->token.length);
    dst[p->token.length] = '\0';
    cdb_lower(dst);
    return advance(p);
}
static bool literal(Parser *p, CdbValue *v, bool negative) {
    *v = cdb_null();
    CdbToken t = p->token;
    if (t.type == TK_INTEGER || t.type == TK_FLOAT) {
        if (t.length > 127)
            return cdb_fail(p->error, CDB_ERR_LIMIT, "Numeric literal too long");
        char buf[128];
        memcpy(buf, t.start, t.length);
        buf[t.length] = '\0';
        errno = 0;
        char *end = NULL;
        if (t.type == TK_INTEGER) {
            unsigned long long n = strtoull(buf, &end, 10);
            uint64_t bound = (uint64_t)INT64_MAX + (negative ? 1u : 0u);
            if (errno || !end || *end || n > bound)
                return cdb_fail(p->error, CDB_ERR_TYPE, "INTEGER literal out of range");
            int64_t signed_n =
                negative ? (n == (uint64_t)INT64_MAX + 1u ? INT64_MIN : -(int64_t)n) : (int64_t)n;
            *v = cdb_integer(signed_n);
        } else {
            double n = strtod(buf, &end);
            if (errno || !end || *end || !isfinite(n))
                return cdb_fail(p->error, CDB_ERR_TYPE, "FLOAT literal out of range");
            *v = cdb_float(negative ? -n : n);
        }
    } else {
        if (negative)
            return syntax(p, "number after sign");
        if (t.type == TK_NULL)
            *v = cdb_null();
        else if (t.type == TK_TRUE || t.type == TK_FALSE)
            *v = cdb_boolean(t.type == TK_TRUE);
        else if (t.type == TK_STRING) {
            char text[CDB_MAX_RECORD + 1];
            size_t n = 0;
            for (size_t i = 1; i + 1 < t.length; i++) {
                if (n >= CDB_MAX_RECORD)
                    return cdb_fail(p->error, CDB_ERR_LIMIT, "String exceeds row size limit");
                text[n++] = t.start[i];
                if (t.start[i] == '\'' && i + 2 < t.length && t.start[i + 1] == '\'')
                    i++;
            }
            CDB_TRY(cdb_text(v, text, n, p->error));
        } else
            return syntax(p, "literal value");
    }
    if (!advance(p)) {
        cdb_value_free(v);
        return false;
    }
    return true;
}
static CdbExpr *node(Parser *p, CdbExprKind kind) {
    if (++p->nodes > 256) {
        cdb_fail(p->error, CDB_ERR_LIMIT, "Expression exceeds 256 nodes");
        return NULL;
    }
    CdbExpr *x = calloc(1, sizeof(*x));
    if (!x) {
        cdb_fail(p->error, CDB_ERR_MEMORY, "Cannot allocate expression");
        return NULL;
    }
    x->kind = kind;
    x->binding = -1;
    return x;
}
static unsigned precedence(CdbTokenType op) {
    switch (op) {
    case TK_OR:
        return 1;
    case TK_AND:
        return 2;
    case TK_EQ:
    case TK_NE:
    case TK_LT:
    case TK_LE:
    case TK_GT:
    case TK_GE:
    case TK_IS:
        return 4;
    case TK_PLUS:
    case TK_MINUS:
        return 5;
    case TK_STAR:
    case TK_SLASH:
        return 6;
    default:
        return 0;
    }
}
static CdbExpr *expression(Parser *p, unsigned min_prec);
static CdbExpr *primary(Parser *p) {
    CdbTokenType type = p->token.type;
    if (type == TK_LPAREN) {
        if (!advance(p))
            return NULL;
        CdbExpr *x = expression(p, 1);
        if (!x)
            return NULL;
        if (!expect(p, TK_RPAREN, "')'")) {
            cdb_expr_free(x);
            return NULL;
        }
        return x;
    }
    if (type == TK_MINUS || type == TK_PLUS || type == TK_NOT) {
        if (!advance(p))
            return NULL;
        if ((type == TK_MINUS || type == TK_PLUS) &&
            (p->token.type == TK_INTEGER || p->token.type == TK_FLOAT)) {
            CdbExpr *x = node(p, EX_VALUE);
            if (!x)
                return NULL;
            if (!literal(p, &x->value, type == TK_MINUS)) {
                cdb_expr_free(x);
                return NULL;
            }
            return x;
        }
        CdbExpr *x = node(p, EX_UNARY);
        if (!x)
            return NULL;
        x->op = type;
        x->left = expression(p, type == TK_NOT ? 3u : 7u);
        if (!x->left) {
            cdb_expr_free(x);
            return NULL;
        }
        return x;
    }
    CdbExpr *x = node(p, type == TK_IDENT ? EX_COLUMN : EX_VALUE);
    if (!x)
        return NULL;
    bool ok = type == TK_IDENT ? name(p, x->column) : literal(p, &x->value, false);
    if (!ok) {
        cdb_expr_free(x);
        return NULL;
    }
    return x;
}
static CdbExpr *expression(Parser *p, unsigned min_prec) {
    if (++p->depth > CDB_MAX_EXPR_DEPTH) {
        cdb_fail(p->error, CDB_ERR_LIMIT, "Expression nesting exceeds %u", CDB_MAX_EXPR_DEPTH);
        p->depth--;
        return NULL;
    }
    CdbExpr *left = primary(p);
    if (!left) {
        p->depth--;
        return NULL;
    }
    while (precedence(p->token.type) >= min_prec) {
        CdbTokenType op = p->token.type;
        unsigned prec = precedence(op);
        if (!advance(p)) {
            cdb_expr_free(left);
            p->depth--;
            return NULL;
        }
        CdbExpr *x = node(p, op == TK_IS ? EX_UNARY : EX_BINARY);
        if (!x) {
            cdb_expr_free(left);
            p->depth--;
            return NULL;
        }
        x->left = left;
        x->op = op;
        if (op == TK_IS) {
            if (p->token.type == TK_NOT) {
                x->op = TK_NOT;
                if (!advance(p)) {
                    cdb_expr_free(x);
                    p->depth--;
                    return NULL;
                }
            }
            /* IS NULL and IS NOT NULL use binding as an explicit unary discriminator. */ x
                ->binding = 1;
            if (!expect(p, TK_NULL, "NULL after IS [NOT]")) {
                cdb_expr_free(x);
                p->depth--;
                return NULL;
            }
        } else {
            x->right = expression(p, prec + 1);
            if (!x->right) {
                cdb_expr_free(x);
                p->depth--;
                return NULL;
            }
        }
        left = x;
    }
    p->depth--;
    return left;
}
static bool where_clause(Parser *p, CdbQuery *q) {
    if (p->token.type != TK_WHERE)
        return true;
    CDB_TRY(advance(p));
    q->where = expression(p, 1);
    return q->where != NULL;
}
static bool names(Parser *p, CdbQuery *q) {
    do {
        if (q->name_count == CDB_MAX_COLUMNS)
            return cdb_fail(p->error, CDB_ERR_LIMIT, "Too many column names");
        CDB_TRY(name(p, q->names[q->name_count++]));
        if (p->token.type != TK_COMMA)
            break;
        CDB_TRY(advance(p));
    } while (true);
    return true;
}
static bool parse_create(Parser *p, CdbQuery *q) {
    CDB_TRY(advance(p));
    if (p->token.type == TK_INDEX) {
        q->kind = Q_INDEX;
        CDB_TRY(advance(p));
        CDB_TRY(name(p, q->index_name));
        CDB_TRY(expect(p, TK_ON, "ON"));
        CDB_TRY(name(p, q->table));
        CDB_TRY(expect(p, TK_LPAREN, "'('"));
        CDB_TRY(name(p, q->names[0]));
        q->name_count = 1;
        return expect(p, TK_RPAREN, "')'");
    }
    q->kind = Q_CREATE;
    CDB_TRY(expect(p, TK_TABLE, "TABLE"));
    CDB_TRY(name(p, q->table));
    CDB_TRY(expect(p, TK_LPAREN, "'('"));
    do {
        if (q->column_count == CDB_MAX_COLUMNS)
            return cdb_fail(p->error, CDB_ERR_LIMIT, "Too many columns");
        CdbColumn *c = &q->columns[q->column_count++];
        CDB_TRY(name(p, c->name));
        switch (p->token.type) {
        case TK_INT_TYPE:
            c->type = CDB_INTEGER;
            break;
        case TK_FLOAT_TYPE:
            c->type = CDB_FLOAT;
            break;
        case TK_TEXT_TYPE:
            c->type = CDB_TEXT;
            break;
        case TK_BOOL_TYPE:
            c->type = CDB_BOOLEAN;
            break;
        default:
            return syntax(p, "column type");
        }
        CDB_TRY(advance(p));
        while (p->token.type == TK_PRIMARY || p->token.type == TK_NOT) {
            if (p->token.type == TK_PRIMARY) {
                if (c->primary)
                    return syntax(p, "one PRIMARY KEY constraint");
                CDB_TRY(advance(p));
                CDB_TRY(expect(p, TK_KEY, "KEY"));
                c->primary = true;
                c->not_null = true;
            } else {
                CDB_TRY(advance(p));
                CDB_TRY(expect(p, TK_NULL, "NULL"));
                c->not_null = true;
            }
        }
        if (p->token.type != TK_COMMA)
            break;
        CDB_TRY(advance(p));
    } while (true);
    return expect(p, TK_RPAREN, "')'");
}
static bool parse_insert(Parser *p, CdbQuery *q) {
    q->kind = Q_INSERT;
    CDB_TRY(advance(p));
    CDB_TRY(expect(p, TK_INTO, "INTO"));
    CDB_TRY(name(p, q->table));
    if (p->token.type == TK_LPAREN) {
        CDB_TRY(advance(p));
        CDB_TRY(names(p, q));
        CDB_TRY(expect(p, TK_RPAREN, "')'"));
    }
    CDB_TRY(expect(p, TK_VALUES, "VALUES"));
    do {
        if (q->row_count == 4096)
            return cdb_fail(p->error, CDB_ERR_LIMIT, "At most 4096 VALUES rows per statement");
        if (q->row_count == q->row_capacity) {
            size_t cap = q->row_capacity ? q->row_capacity * 2 : 8;
            CdbRow *rows = realloc(q->rows, cap * sizeof(*rows));
            if (!rows)
                return cdb_fail(p->error, CDB_ERR_MEMORY, "Cannot allocate VALUES rows");
            q->rows = rows;
            q->row_capacity = cap;
        }
        CdbRow *r = &q->rows[q->row_count++];
        memset(r, 0, sizeof(*r));
        CDB_TRY(expect(p, TK_LPAREN, "'('"));
        do {
            if (r->count == CDB_MAX_COLUMNS)
                return cdb_fail(p->error, CDB_ERR_LIMIT, "Too many values");
            bool negative = p->token.type == TK_MINUS;
            if (negative || p->token.type == TK_PLUS) {
                CDB_TRY(advance(p));
                if (p->token.type != TK_INTEGER && p->token.type != TK_FLOAT)
                    return syntax(p, "number after sign");
            }
            CDB_TRY(literal(p, &r->values[r->count++], negative));
            if (p->token.type != TK_COMMA)
                break;
            CDB_TRY(advance(p));
        } while (true);
        CDB_TRY(expect(p, TK_RPAREN, "')'"));
        if (p->token.type != TK_COMMA)
            break;
        CDB_TRY(advance(p));
    } while (true);
    return true;
}
static bool statement(Parser *p, CdbQuery *q) {
    if (p->token.type == TK_EXPLAIN) {
        q->explain = true;
        CDB_TRY(advance(p));
        if (p->token.type != TK_SELECT)
            return syntax(p, "SELECT after EXPLAIN");
    }
    switch (p->token.type) {
    case TK_CREATE:
        return parse_create(p, q);
    case TK_DROP:
        q->kind = Q_DROP;
        CDB_TRY(advance(p));
        CDB_TRY(expect(p, TK_TABLE, "TABLE"));
        return name(p, q->table);
    case TK_INSERT:
        return parse_insert(p, q);
    case TK_SELECT:
        q->kind = Q_SELECT;
        CDB_TRY(advance(p));
        if (p->token.type == TK_STAR) {
            q->star = true;
            CDB_TRY(advance(p));
        } else
            CDB_TRY(names(p, q));
        CDB_TRY(expect(p, TK_FROM, "FROM"));
        CDB_TRY(name(p, q->table));
        CDB_TRY(where_clause(p, q));
        if (p->token.type == TK_LIMIT) {
            CDB_TRY(advance(p));
            if (p->token.type != TK_INTEGER)
                return syntax(p, "nonnegative integer LIMIT");
            CdbValue v;
            CDB_TRY(literal(p, &v, false));
            if ((uint64_t)v.as.integer > CDB_MAX_RESULT_ROWS)
                return cdb_fail(p->error, CDB_ERR_LIMIT, "LIMIT exceeds result cap");
            q->limit = (size_t)v.as.integer;
        }
        return true;
    case TK_UPDATE:
        q->kind = Q_UPDATE;
        CDB_TRY(advance(p));
        CDB_TRY(name(p, q->table));
        CDB_TRY(expect(p, TK_SET, "SET"));
        do {
            if (q->assignment_count == CDB_MAX_COLUMNS)
                return cdb_fail(p->error, CDB_ERR_LIMIT, "Too many assignments");
            CdbAssignment *a = &q->assignments[q->assignment_count++];
            CDB_TRY(name(p, a->column));
            CDB_TRY(expect(p, TK_EQ, "'='"));
            a->expr = expression(p, 1);
            if (!a->expr)
                return false;
            if (p->token.type != TK_COMMA)
                break;
            CDB_TRY(advance(p));
        } while (true);
        return where_clause(p, q);
    case TK_DELETE:
        q->kind = Q_DELETE;
        CDB_TRY(advance(p));
        CDB_TRY(expect(p, TK_FROM, "FROM"));
        CDB_TRY(name(p, q->table));
        return where_clause(p, q);
    case TK_BEGIN:
        q->kind = Q_BEGIN;
        return advance(p);
    case TK_COMMIT:
        q->kind = Q_COMMIT;
        return advance(p);
    case TK_ROLLBACK:
        q->kind = Q_ROLLBACK;
        return advance(p);
    default:
        return syntax(p, "CREATE, DROP, INSERT, SELECT, UPDATE, DELETE, BEGIN, COMMIT or ROLLBACK");
    }
}
bool cdb_parse(const char *sql, CdbQuery *q, CdbError *e) {
    memset(q, 0, sizeof(*q));
    q->limit = SIZE_MAX;
    cdb_error_clear(e);
    if (!sql)
        return cdb_fail(e, CDB_ERR_SYNTAX, "SQL is NULL");
    if (strlen(sql) > CDB_MAX_SQL)
        return cdb_fail(e, CDB_ERR_LIMIT, "SQL exceeds 1 MiB");
    Parser p = {.error = e};
    cdb_lexer_init(&p.lexer, sql);
    bool ok = advance(&p) && statement(&p, q);
    if (ok && p.token.type == TK_SEMI)
        ok = advance(&p);
    if (ok && p.token.type != TK_EOF)
        ok = syntax(&p, "end of statement");
    if (!ok)
        cdb_query_free(q);
    return ok;
}
