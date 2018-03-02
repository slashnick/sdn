#ifndef TREE_MAP_H_
#define TREE_MAP_H_

#include <stdint.h>

typedef struct tree_map {
    struct tree_map *parent;
    struct tree_map *left;
    struct tree_map *right;
    uint64_t key;
    uint64_t value;
    uint8_t color; /* Used by insert to maintain balance */
} tree_map_t;

tree_map_t *tm_set(tree_map_t *, uint64_t, uint64_t)
    __attribute__((warn_unused_result));
uint64_t tm_get(const tree_map_t *, uint64_t);
uint8_t tm_contains(const tree_map_t *, uint64_t);
void tm_free(tree_map_t *);

#endif /* TREE_MAP_H_ */
