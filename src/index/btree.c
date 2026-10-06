#include "cdb/btree.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
struct CdbBtreeNode {
    bool leaf;
    unsigned count;
    CdbEntry keys[CDB_BTREE_KEYS];
    CdbBtreeNode *children[CDB_BTREE_KEYS + 1u];
};
static int compare(CdbEntry a, CdbEntry b) {
    if (a.key != b.key)
        return (a.key > b.key) ? 1 : -1;
    return (a.rid > b.rid) - (a.rid < b.rid);
}
static CdbBtreeNode *node_new(bool leaf, CdbError *e) {
    CdbBtreeNode *n = calloc(1, sizeof(*n));
    if (!n) {
        cdb_fail(e, CDB_ERR_MEMORY, "Cannot allocate B-Tree node");
        return NULL;
    }
    n->leaf = leaf;
    return n;
}
static void node_free(CdbBtreeNode *n) {
    if (!n)
        return;
    if (!n->leaf)
        for (unsigned i = 0; i <= n->count; i++)
            node_free(n->children[i]);
    free(n);
}
void cdb_btree_free(CdbBtree *t) {
    node_free(t->root);
    memset(t, 0, sizeof(*t));
}
static unsigned lower_bound(const CdbBtreeNode *n, CdbEntry key) {
    unsigned lo = 0, hi = n->count;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (compare(n->keys[mid], key) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}
static bool contains(const CdbBtreeNode *n, CdbEntry key) {
    while (n) {
        unsigned i = lower_bound(n, key);
        if (i < n->count && compare(n->keys[i], key) == 0)
            return true;
        if (n->leaf)
            return false;
        n = n->children[i];
    }
    return false;
}
bool cdb_btree_find(const CdbBtree *t, int64_t key, CdbRid *rid) {
    const CdbBtreeNode *n = t->root;
    while (n) {
        unsigned i = lower_bound(n, (CdbEntry){key, 0});
        if (i < n->count && n->keys[i].key == key) {
            if (rid)
                *rid = n->keys[i].rid;
            return true;
        }
        if (n->leaf)
            return false;
        n = n->children[i];
    }
    return false;
}
/* Allocate first; an allocation failure must not leave a half-split node. */ static bool
split(CdbBtreeNode *parent, unsigned i, CdbError *e) {
    CdbBtreeNode *left = parent->children[i], *right = node_new(left->leaf, e);
    if (!right)
        return false;
    right->count = CDB_BTREE_DEGREE - 1u;
    for (unsigned j = 0; j < right->count; j++)
        right->keys[j] = left->keys[j + CDB_BTREE_DEGREE];
    if (!left->leaf)
        for (unsigned j = 0; j < CDB_BTREE_DEGREE; j++)
            right->children[j] = left->children[j + CDB_BTREE_DEGREE];
    for (unsigned j = parent->count + 1u; j > i + 1u; j--)
        parent->children[j] = parent->children[j - 1u];
    parent->children[i + 1u] = right;
    for (unsigned j = parent->count; j > i; j--)
        parent->keys[j] = parent->keys[j - 1u];
    parent->keys[i] = left->keys[CDB_BTREE_DEGREE - 1u];
    left->count = CDB_BTREE_DEGREE - 1u;
    parent->count++;
    return true;
}
static bool insert_nonfull(CdbBtreeNode *n, CdbEntry key, CdbError *e) {
    unsigned i = lower_bound(n, key);
    if (n->leaf) {
        for (unsigned j = n->count; j > i; j--)
            n->keys[j] = n->keys[j - 1u];
        n->keys[i] = key;
        n->count++;
        return true;
    }
    if (n->children[i]->count == CDB_BTREE_KEYS) {
        CDB_TRY(split(n, i, e));
        if (compare(key, n->keys[i]) > 0)
            i++;
    }
    return insert_nonfull(n->children[i], key, e);
}
bool cdb_btree_insert(CdbBtree *t, int64_t key, CdbRid rid, CdbError *e) {
    CdbEntry entry = {key, rid};
    if (contains(t->root, entry))
        return cdb_fail(e, CDB_ERR_EXISTS, "Duplicate B-Tree entry");
    if (!t->root) {
        t->root = node_new(true, e);
        if (!t->root)
            return false;
    }
    if (t->root->count == CDB_BTREE_KEYS) {
        CdbBtreeNode *root = node_new(false, e);
        if (!root)
            return false;
        root->children[0] = t->root;
        if (!split(root, 0, e)) {
            free(root);
            return false;
        }
        t->root = root;
    }
    CDB_TRY(insert_nonfull(t->root, entry, e));
    t->count++;
    return true;
}
static bool equal_node(const CdbBtreeNode *n, int64_t key, CdbIndexVisitor visit, void *ctx,
                       CdbError *e) {
    if (!n)
        return true;
    unsigned i = lower_bound(n, (CdbEntry){key, 0});
    if (!n->leaf)
        CDB_TRY(equal_node(n->children[i], key, visit, ctx, e));
    while (i < n->count && n->keys[i].key == key) {
        CDB_TRY(visit(n->keys[i].rid, ctx, e));
        i++;
        if (!n->leaf)
            CDB_TRY(equal_node(n->children[i], key, visit, ctx, e));
    }
    return true;
}
bool cdb_btree_equal(const CdbBtree *t, int64_t key, CdbIndexVisitor visit, void *ctx,
                     CdbError *e) {
    return equal_node(t->root, key, visit, ctx, e);
}
static void merge(CdbBtreeNode *p, unsigned i) {
    CdbBtreeNode *a = p->children[i], *b = p->children[i + 1u];
    unsigned old = a->count;
    a->keys[old] = p->keys[i];
    for (unsigned j = 0; j < b->count; j++)
        a->keys[old + 1u + j] = b->keys[j];
    if (!a->leaf)
        for (unsigned j = 0; j <= b->count; j++)
            a->children[old + 1u + j] = b->children[j];
    a->count += b->count + 1u;
    for (unsigned j = i + 1u; j < p->count; j++)
        p->keys[j - 1u] = p->keys[j];
    for (unsigned j = i + 2u; j <= p->count; j++)
        p->children[j - 1u] = p->children[j];
    p->count--;
    free(b);
}
static void borrow_left(CdbBtreeNode *p, unsigned i) {
    CdbBtreeNode *n = p->children[i], *s = p->children[i - 1u];
    for (unsigned j = n->count; j > 0; j--)
        n->keys[j] = n->keys[j - 1u];
    if (!n->leaf) {
        for (unsigned j = n->count + 1u; j > 0; j--)
            n->children[j] = n->children[j - 1u];
        n->children[0] = s->children[s->count];
    }
    n->keys[0] = p->keys[i - 1u];
    p->keys[i - 1u] = s->keys[s->count - 1u];
    n->count++;
    s->count--;
}
static void borrow_right(CdbBtreeNode *p, unsigned i) {
    CdbBtreeNode *n = p->children[i], *s = p->children[i + 1u];
    n->keys[n->count] = p->keys[i];
    if (!n->leaf)
        n->children[n->count + 1u] = s->children[0];
    p->keys[i] = s->keys[0];
    for (unsigned j = 1; j < s->count; j++)
        s->keys[j - 1u] = s->keys[j];
    if (!s->leaf)
        for (unsigned j = 1; j <= s->count; j++)
            s->children[j - 1u] = s->children[j];
    n->count++;
    s->count--;
}
static CdbEntry extreme(CdbBtreeNode *n, bool maximum) {
    while (!n->leaf)
        n = n->children[maximum ? n->count : 0];
    return n->keys[maximum ? n->count - 1u : 0];
}
static void remove_entry(CdbBtreeNode *n, CdbEntry key) {
    unsigned i = lower_bound(n, key);
    if (i < n->count && compare(n->keys[i], key) == 0) {
        if (n->leaf) {
            for (unsigned j = i + 1u; j < n->count; j++)
                n->keys[j - 1u] = n->keys[j];
            n->count--;
            return;
        }
        if (n->children[i]->count >= CDB_BTREE_DEGREE) {
            CdbEntry pred = extreme(n->children[i], true);
            n->keys[i] = pred;
            remove_entry(n->children[i], pred);
        } else if (n->children[i + 1u]->count >= CDB_BTREE_DEGREE) {
            CdbEntry next = extreme(n->children[i + 1u], false);
            n->keys[i] = next;
            remove_entry(n->children[i + 1u], next);
        } else {
            merge(n, i);
            remove_entry(n->children[i], key);
        }
        return;
    }
    if (n->leaf)
        return;
    /* Descend only into a child with at least t keys, so deletion cannot underflow it. */
    if (n->children[i]->count < CDB_BTREE_DEGREE) {
        if (i > 0 && n->children[i - 1u]->count >= CDB_BTREE_DEGREE)
            borrow_left(n, i);
        else if (i < n->count && n->children[i + 1u]->count >= CDB_BTREE_DEGREE)
            borrow_right(n, i);
        else if (i < n->count)
            merge(n, i);
        else {
            merge(n, i - 1u);
            i--;
        }
    }
    remove_entry(n->children[i], key);
}
bool cdb_btree_delete(CdbBtree *t, int64_t key, CdbRid rid) {
    CdbEntry entry = {key, rid};
    if (!contains(t->root, entry))
        return false;
    remove_entry(t->root, entry);
    t->count--;
    if (t->root->count == 0) {
        CdbBtreeNode *old = t->root;
        t->root = old->leaf ? NULL : old->children[0];
        free(old);
    }
    return true;
}
static bool validate_node(const CdbBtreeNode *n, bool root, unsigned depth, unsigned *leaf_depth,
                          const CdbEntry *lo, const CdbEntry *hi, size_t *count, CdbError *e) {
    if (!n || depth > 64 || n->count > CDB_BTREE_KEYS ||
        (!root && n->count < CDB_BTREE_DEGREE - 1u) || !n->count)
        return cdb_fail(e, CDB_ERR_CORRUPT, "B-Tree occupancy/depth violation");
    for (unsigned i = 0; i < n->count; i++) {
        if ((lo && compare(n->keys[i], *lo) <= 0) || (hi && compare(n->keys[i], *hi) >= 0) ||
            (i && compare(n->keys[i - 1u], n->keys[i]) >= 0))
            return cdb_fail(e, CDB_ERR_CORRUPT, "B-Tree ordering violation");
    }
    *count += n->count;
    if (n->leaf) {
        if (*leaf_depth == UINT32_MAX)
            *leaf_depth = depth;
        return depth == *leaf_depth || cdb_fail(e, CDB_ERR_CORRUPT, "B-Tree leaf depths differ");
    }
    for (unsigned i = 0; i <= n->count; i++)
        CDB_TRY(validate_node(n->children[i], false, depth + 1u, leaf_depth,
                              i ? &n->keys[i - 1u] : lo, i < n->count ? &n->keys[i] : hi, count,
                              e));
    return true;
}
bool cdb_btree_validate(const CdbBtree *t, CdbError *e) {
    if (!t->root)
        return t->count == 0 || cdb_fail(e, CDB_ERR_CORRUPT, "B-Tree empty-root count mismatch");
    unsigned depth = UINT32_MAX;
    size_t count = 0;
    CDB_TRY(validate_node(t->root, true, 0, &depth, NULL, NULL, &count, e));
    return count == t->count || cdb_fail(e, CDB_ERR_CORRUPT, "B-Tree count mismatch");
}
static void print_node(const CdbBtreeNode *n, unsigned depth, FILE *out) {
    if (!n)
        return;
    for (unsigned i = 0; i < depth; i++)
        fputs("  ", out);
    fputc('[', out);
    for (unsigned i = 0; i < n->count; i++)
        fprintf(out, "%s%" PRId64 ":%" PRIu64, i ? ", " : "", n->keys[i].key, n->keys[i].rid);
    fputs("]\n", out);
    if (!n->leaf)
        for (unsigned i = 0; i <= n->count; i++)
            print_node(n->children[i], depth + 1u, out);
}
void cdb_btree_print(const CdbBtree *t, FILE *out) {
    if (!t->root)
        fputs("(empty)\n", out);
    else
        print_node(t->root, 0, out);
}
