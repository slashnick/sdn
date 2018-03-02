#include <assert.h>
#include <stdio.h>
#include "../tree_map.h"

enum {
    RED,
    BLACK,
};

static void assert_tree(tree_map_t *map) {
    if (map == NULL) {
        return;
    }
    if (map->left != NULL) {
        assert(map->left->key < map->key);
        assert(map->left->parent == map);
        assert_tree(map->left);
    }
    if (map->right != NULL) {
        assert(map->right->key > map->key);
        assert(map->right->parent == map);
        assert_tree(map->right);
    }
}

static void test1(void) {
    tree_map_t *map = NULL;

    tm_free(NULL);

    assert(!tm_contains(map, 2));
    assert(tm_get(map, 2) == 0);
    map = tm_set(map, 2, 200);
    assert(tm_contains(map, 2));
    assert(tm_get(map, 2) == 200);
    assert_tree(map);

    map = tm_set(map, 2, 201);
    assert(map->left == NULL);
    assert(map->right == NULL);
    assert(tm_get(map, 2) == 201);

    assert(!tm_contains(map, 0));
    assert(tm_get(map, 0) == 0);
    map = tm_set(map, 0, 1);
    assert(tm_contains(map, 0));
    assert(tm_get(map, 0) == 1);

    assert(!tm_contains(map, 3));
    map = tm_set(map, 3, 300);
    assert(tm_contains(map, 3));
    assert(tm_get(map, 3) == 300);

    assert(!tm_contains(map, 1));
    map = tm_set(map, 1, 100);
    assert(tm_contains(map, 1));
    assert(tm_get(map, 1) == 100);

    assert(!tm_contains(map, 4));
    map = tm_set(map, 4, 400);
    assert(tm_contains(map, 4));
    assert(tm_get(map, 4) == 400);

    assert_tree(map);

    tm_free(map);
    map = NULL;
}

// Really test out the red-black rebalancing
static void test2(void) {
    tree_map_t *map = NULL;

    map = tm_set(map, 15, 0);
    map = tm_set(map, 1, 0);
    map = tm_set(map, 13, 0);
    map = tm_set(map, 9, 0);
    map = tm_set(map, 11, 0);
    map = tm_set(map, 4, 0);
    map = tm_set(map, 10, 0);
    map = tm_set(map, 8, 0);
    map = tm_set(map, 6, 0);
    map = tm_set(map, 19, 0);
    map = tm_set(map, 2, 0);
    map = tm_set(map, 12, 0);
    map = tm_set(map, 18, 0);
    map = tm_set(map, 14, 0);
    map = tm_set(map, 17, 0);
    map = tm_set(map, 0, 0);
    map = tm_set(map, 3, 0);
    map = tm_set(map, 5, 0);
    map = tm_set(map, 16, 0);
    map = tm_set(map, 7, 0);

    assert_tree(map);

    assert(map->key == 9);
    assert(map->color == BLACK);
    assert(map->left->key == 4);
    assert(map->left->color == BLACK);
    assert(map->left->left->key == 1);
    assert(map->left->left->color == RED);
    assert(map->left->left->left->key == 0);
    assert(map->left->left->left->color == BLACK);
    assert(map->left->left->right->key == 2);
    assert(map->left->left->right->color == BLACK);
    assert(map->left->left->right->right->key == 3);
    assert(map->left->left->right->right->color == RED);
    assert(map->left->right->key == 6);
    assert(map->left->right->color == RED);
    assert(map->left->right->left->key == 5);
    assert(map->left->right->left->color == BLACK);
    assert(map->left->right->right->key == 8);
    assert(map->left->right->right->color == BLACK);
    assert(map->left->right->right->left->key == 7);
    assert(map->left->right->right->left->color == RED);
    assert(map->right->key == 15);
    assert(map->right->color == BLACK);
    assert(map->right->left->key == 13);
    assert(map->right->left->color == RED);
    assert(map->right->left->left->key == 11);
    assert(map->right->left->left->color == BLACK);
    assert(map->right->left->left->left->key == 10);
    assert(map->right->left->left->left->color == RED);
    assert(map->right->left->left->right->key == 12);
    assert(map->right->left->left->right->color == RED);
    assert(map->right->left->right->key == 14);
    assert(map->right->left->right->color == BLACK);
    assert(map->right->right->key == 18);
    assert(map->right->right->color == RED);
    assert(map->right->right->left->key == 17);
    assert(map->right->right->left->color == BLACK);
    assert(map->right->right->left->left->key == 16);
    assert(map->right->right->left->left->color == RED);
    assert(map->right->right->right->key == 19);
    assert(map->right->right->right->color == BLACK);

    tm_free(map);
}

static void test3(void) {
    tree_map_t *map = NULL;

    map = tm_set(map, 15, 0);
    assert(map->key == 15);
    assert(map->color == BLACK);

    map = tm_set(map, 1, 0);
    assert(map->key == 15);
    assert(map->color == BLACK);
    assert(map->left->key == 1);
    assert(map->left->color == RED);

    map = tm_set(map, 13, 0);
    assert(map->key == 13);
    assert(map->color == BLACK);
    assert(map->left->key == 1);
    assert(map->left->color == RED);
    assert(map->right->key == 15);
    assert(map->right->color == RED);

    map = tm_set(map, 9, 0);
    assert(map->left->color == BLACK);
    assert(map->left->right->key == 9);
    assert(map->left->right->color == RED);
    assert(map->right->color == BLACK);

    map = tm_set(map, 11, 0);
    assert(map->key == 13);
    map = tm_set(map, 4, 0);
    assert(map->key == 13);
    map = tm_set(map, 10, 0);
    assert(map->key == 13);
    assert(map->color == BLACK);
    assert(map->left->key == 9);
    assert(map->left->color == RED);
    assert(map->left->left->key == 1);
    assert(map->left->left->color == BLACK);
    assert(map->left->left->right->key == 4);
    assert(map->left->left->right->color == RED);
    assert(map->left->right->key == 11);
    assert(map->left->right->color == BLACK);
    assert(map->left->right->left->key == 10);
    assert(map->left->right->left->color == RED);
    assert(map->right->key == 15);
    assert(map->right->color == BLACK);

    assert_tree(map);
    map = tm_set(map, 8, 0);
    assert_tree(map);
    assert(map->key == 13);
    assert(map->color == BLACK);
    assert(map->left->key == 9);
    assert(map->left->color == RED);

    map = tm_set(map, 6, 0);
    map = tm_set(map, 19, 0);
    map = tm_set(map, 2, 0);
    map = tm_set(map, 12, 0);
    assert_tree(map);
    assert(map->key == 9);

    map = tm_set(map, 18, 0);
    map = tm_set(map, 14, 0);
    map = tm_set(map, 17, 0);
    assert(map->key == 9);

    tm_free(map);
}

int main(void) {
    test1();
    test2();
    test3();
    printf("\033[0;32mAll tests passed.\033[0m\n");

    return 0;
}
