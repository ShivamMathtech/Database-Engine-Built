#ifndef CDB_AST_H
#define CDB_AST_H
#include "lexer.h"
#include "table.h"
typedef enum { EX_VALUE, EX_COLUMN, EX_UNARY, EX_BINARY } CdbExprKind;
typedef struct CdbExpr {
    CdbExprKind kind;
    CdbTokenType op;
    CdbValue value;
    char column[CDB_NAME_MAX + 1];
    int binding;
    struct CdbExpr *left, *right;
} CdbExpr;
typedef enum {
    Q_CREATE,
    Q_DROP,
    Q_INSERT,
    Q_SELECT,
    Q_UPDATE,
    Q_DELETE,
    Q_INDEX,
    Q_BEGIN,
    Q_COMMIT,
    Q_ROLLBACK
} CdbQueryKind;
typedef struct {
    char column[CDB_NAME_MAX + 1];
    CdbExpr *expr;
    int binding;
} CdbAssignment;
typedef struct {
    CdbQueryKind kind;
    char table[CDB_NAME_MAX + 1];
    char index_name[CDB_NAME_MAX + 1];
    CdbColumn columns[CDB_MAX_COLUMNS];
    uint16_t column_count;
    char names[CDB_MAX_COLUMNS][CDB_NAME_MAX + 1];
    uint16_t name_count;
    bool star, explain;
    CdbRow *rows;
    size_t row_count, row_capacity;
    CdbExpr *where;
    CdbAssignment assignments[CDB_MAX_COLUMNS];
    uint16_t assignment_count;
    size_t limit;
} CdbQuery;
void cdb_expr_free(CdbExpr *expr);
void cdb_query_free(CdbQuery *query);
#endif
