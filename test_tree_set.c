#include <assert.h>
#include <stdio.h>
#include "tree_set.h"

static void test1(void) {
    tree_set_t *set = NULL;

    assert(!ts_contains(set, 2));
    set = ts_insert(set, 2);
    assert(ts_contains(set, 2));

    set = ts_insert(set, 2);
    assert(set->left == NULL);
    assert(set->right == NULL);

    assert(!ts_contains(set, 0));
    set = ts_insert(set, 0);
    assert(ts_contains(set, 0));

    assert(!ts_contains(set, 3));
    set = ts_insert(set, 3);
    assert(ts_contains(set, 3));

    assert(!ts_contains(set, 1));
    set = ts_insert(set, 1);
    assert(ts_contains(set, 1));

    assert(!ts_contains(set, 4));
    set = ts_insert(set, 4);
    assert(ts_contains(set, 4));

    ts_free(set);
}

int main(void) {
    test1();

    return 0;
}
