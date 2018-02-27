#ifndef TREE_SET_H_
#define TREE_SET_H_

#include <stdint.h>

typedef struct tree_set {
    struct tree_set *left;
    struct tree_set *right;
    uint64_t val;
} tree_set_t;

tree_set_t *ts_insert(tree_set_t *, uint64_t);
uint8_t ts_contains(tree_set_t *, uint64_t);
void ts_free(tree_set_t *);

#endif /* TREE_SET_H_ */
