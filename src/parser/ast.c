#include "cdb/ast.h"
#include <stdlib.h>
#include <string.h>
void cdb_expr_free(CdbExpr *x) {
    if (!x)
        return;
    cdb_expr_free(x->left);
    cdb_expr_free(x->right);
    cdb_value_free(&x->value);
    free(x);
}
void cdb_query_free(CdbQuery *q) {
    cdb_expr_free(q->where);
    for (uint16_t i = 0; i < q->assignment_count; i++)
        cdb_expr_free(q->assignments[i].expr);
    for (size_t i = 0; i < q->row_count; i++)
        cdb_row_free(&q->rows[i]);
    free(q->rows);
    memset(q, 0, sizeof(*q));
}
