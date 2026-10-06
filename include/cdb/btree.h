#ifndef CDB_BTREE_H
#define CDB_BTREE_H
#include "row.h"
#define CDB_BTREE_DEGREE 8u
#define CDB_BTREE_KEYS (2u * CDB_BTREE_DEGREE - 1u)
typedef struct {
    int64_t key;
    CdbRid rid;
} CdbEntry;
typedef struct CdbBtreeNode CdbBtreeNode;
typedef struct {
    CdbBtreeNode *root;
    size_t count;
} CdbBtree;
typedef bool (*CdbIndexVisitor)(CdbRid rid, void *ctx, CdbError *e);
void cdb_btree_free(CdbBtree *tree);
bool cdb_btree_insert(CdbBtree *tree, int64_t key, CdbRid rid, CdbError *e);
bool cdb_btree_find(const CdbBtree *tree, int64_t key, CdbRid *rid);
bool cdb_btree_equal(const CdbBtree *tree, int64_t key, CdbIndexVisitor visit, void *ctx,
                     CdbError *e);
bool cdb_btree_delete(CdbBtree *tree, int64_t key, CdbRid rid);
bool cdb_btree_validate(const CdbBtree *tree, CdbError *e);
void cdb_btree_print(const CdbBtree *tree, FILE *out);
#endif
