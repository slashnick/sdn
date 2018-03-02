/*
 * This module implements a key-value map with a red-black BST
 */

#include "tree_map.h"
#include <stdio.h>
#include <stdlib.h>

static tree_map_t *alloc_node(tree_map_t *, uint64_t, uint64_t, uint8_t);
static tree_map_t *insert_r(tree_map_t *, tree_map_t **, uint64_t, uint64_t);
static tree_map_t *sibling(const tree_map_t *);
static tree_map_t *uncle(const tree_map_t *);
static tree_map_t *grandparent(const tree_map_t *);
static void repair_tree(tree_map_t *);
static void case3(tree_map_t *);
static void case4(tree_map_t *);
static void case4_part2(tree_map_t *);
static void rotate_left(tree_map_t *);
static void rotate_right(tree_map_t *);

enum color {
    RED,
    BLACK,
};

tree_map_t *alloc_node(tree_map_t *parent, uint64_t key, uint64_t value,
                       uint8_t color) {
    tree_map_t *node;

    if ((node = malloc(sizeof(tree_map_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
    node->parent = parent;
    node->left = NULL;
    node->right = NULL;
    node->key = key;
    node->value = value;
    node->color = color;

    return node;
}

uint64_t tm_get(const tree_map_t *map, uint64_t key) {
    if (map == NULL) {
        return 0;
    } else if (key == map->key) {
        return map->value;
    } else if (key < map->key) {
        return tm_get(map->left, key);
    } else {
        return tm_get(map->right, key);
    }
}

uint8_t tm_contains(const tree_map_t *map, uint64_t key) {
    if (map == NULL) {
        return 0;
    } else if (key == map->key) {
        return 1;
    } else if (key < map->key) {
        return tm_contains(map->left, key);
    } else {
        return tm_contains(map->right, key);
    }
}

void tm_free(tree_map_t *map) {
    if (map == NULL) {
        return;
    }
    tm_free(map->left);
    map->left = NULL;
    tm_free(map->right);
    map->right = NULL;
    free(map);
}

tree_map_t *tm_set(tree_map_t *map, uint64_t key, uint64_t value) {
    tree_map_t *node;

    node = insert_r(NULL, &map, key, value);
    if (node != NULL) {
        // Only repair the tree if an actual insert happened
        repair_tree(node);
    } else {
        node = map;
    }
    while (node->parent != NULL) {
        node = node->parent;
    }
    return node;
}

/* Normal recursive BST insert
 *
 * Return new node
 * Return NULL if no new node was created
 */
tree_map_t *insert_r(tree_map_t *parent, tree_map_t **map, uint64_t key,
                     uint64_t value) {
    if (*map == NULL) {
        *map = alloc_node(parent, key, value, RED);
        return *map;
    } else if (key == (*map)->key) {
        (*map)->value = value;
        return NULL;
    } else if (key < (*map)->key) {
        return insert_r(*map, &(*map)->left, key, value);
    } else {
        return insert_r(*map, &(*map)->right, key, value);
    }
}


/*******************************************************************************
 *                               Rebalancing code                              *
 ******************************************************************************/

void repair_tree(tree_map_t *map) {
    if (map->parent == NULL) {
        // Case 1: Node is the parent, just mark as black
        map->color = BLACK;
    } else if (map->parent->color == BLACK) {
        // Case 2: Parent is black, do nothing
    } else if (uncle(map) && uncle(map)->color == RED) {
        case3(map);
    } else {
        case4(map);
    }
}

void case3(tree_map_t *map) {
    map->parent->color = BLACK;
    uncle(map)->color = BLACK;
    grandparent(map)->color = RED;
    repair_tree(grandparent(map));
}

void case4(tree_map_t *map) {
    tree_map_t *gp;

    // This case involves a double rotation, in the event that map is not all
    // the way to the right

    gp = grandparent(map);
    if (gp->left && map == gp->left->right) {
        rotate_left(map->parent);
        map = map->left;
    } else if (gp->right && map == gp->right->left) {
        rotate_right(map->parent);
        map = map->right;
    }
    case4_part2(map);
}

void case4_part2(tree_map_t *map) {
    tree_map_t *p, *gp;

    p = map->parent;
    gp = p->parent;
    // At this point, gp -> p -> map should be a straight line
    if (map == p->left) {
        rotate_right(gp);
    } else {
        rotate_left(gp);
    }
    p->color = BLACK;
    gp->color = RED;
}

void rotate_left(tree_map_t *map) {
    tree_map_t *node = map->right;

    map->right = node->left;
    node->left = map;
    node->parent = map->parent;
    map->parent = node;

    if (node->left != NULL) {
        node->left->parent = node;
    }
    if (map->right != NULL) {
        map->right->parent = map;
    }
    if (node->parent != NULL) {
        if (node->parent->left == map) {
            node->parent->left = node;
        } else {
            node->parent->right = node;
        }
    }
}

void rotate_right(tree_map_t *map) {
    tree_map_t *node = map->left;
    map->left = node->right;
    node->right = map;
    node->parent = map->parent;
    map->parent = node;

    if (node->right != NULL) {
        node->right->parent = node;
    }
    if (map->left != NULL) {
        map->left->parent = map;
    }
    if (node->parent != NULL) {
        if (node->parent->left == map) {
            node->parent->left = node;
        } else {
            node->parent->right = node;
        }
    }
}

tree_map_t *sibling(const tree_map_t *map) {
    if (map == NULL || map->parent == NULL) {
        return NULL;
    }
    if (map == map->parent->left) {
        return map->parent->right;
    } else {
        return map->parent->left;
    }
}

tree_map_t *grandparent(const tree_map_t *map) {
    if (map == NULL || map->parent == NULL) {
        return NULL;
    }
    return map->parent->parent;
}

tree_map_t *uncle(const tree_map_t *map) {
    if (map == NULL) {
        return NULL;
    }
    return sibling(map->parent);
}
