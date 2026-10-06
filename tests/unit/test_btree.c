#include "test.h"
static bool count_hit(CdbRid rid, void *ctx, CdbError *e) {
    (void)rid;
    (void)e;
    (*(size_t *)ctx)++;
    return true;
}
bool test_btree(void) {
    enum { N = 12000 };
    uint32_t *order = malloc(N * sizeof(*order));
    CHECK(order != NULL);
    CdbBtree t = {0};
    CdbError e = {0};
    for (uint32_t i = 0; i < N; i++) {
        order[i] = i;
        OK(cdb_btree_insert(&t, (int64_t)i, i, &e));
    }
    OK(cdb_btree_validate(&t, &e));
    CHECK(!cdb_btree_insert(&t, 5, 5, &e));
    CHECK(t.count == N);
    for (uint32_t i = 0; i < N; i++) {
        CdbRid rid;
        CHECK(cdb_btree_find(&t, i, &rid) && rid == i);
    }
    CHECK(!cdb_btree_find(&t, -1, NULL));
    uint32_t seed = 12345;
    for (size_t i = N - 1; i > 0; i--) {
        size_t j = test_random(&seed) % (i + 1);
        uint32_t v = order[i];
        order[i] = order[j];
        order[j] = v;
    }
    for (size_t i = 0; i < N; i++) {
        CHECK(cdb_btree_delete(&t, order[i], order[i]));
        if (i % 127u == 0)
            OK(cdb_btree_validate(&t, &e));
    }
    CHECK(t.count == 0 && t.root == NULL);
    CHECK(!cdb_btree_delete(&t, 0, 0));
    for (size_t i = 0; i < N; i++)
        OK(cdb_btree_insert(&t, order[i], order[i], &e));
    OK(cdb_btree_validate(&t, &e));
    cdb_btree_free(&t);
    for (uint64_t i = 0; i < 1000; i++)
        OK(cdb_btree_insert(&t, (int64_t)(i % 7u), i, &e));
    size_t count = 0;
    OK(cdb_btree_equal(&t, 3, count_hit, &count, &e));
    CHECK(count == 143);
    OK(cdb_btree_validate(&t, &e));
    for (uint64_t i = 0; i < 1000; i += 2)
        CHECK(cdb_btree_delete(&t, (int64_t)(i % 7u), i));
    OK(cdb_btree_validate(&t, &e));
    count = 0;
    OK(cdb_btree_equal(&t, 3, count_hit, &count, &e));
    CHECK(count == 72);
    cdb_btree_free(&t);
    free(order);
    return true;
}
