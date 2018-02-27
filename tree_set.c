#include "tree_set.h"
#include <stdio.h>
#include <stdlib.h>

tree_set_t *ts_insert(tree_set_t *ts, uint64_t val) {
    if (ts == NULL) {
        if ((ts = malloc(sizeof(tree_set_t))) == NULL) {
            perror("malloc");
            exit(-1);
        }
        ts->left = NULL;
        ts->right = NULL;
        ts->val = val;
    } else if (val < ts->val) {
        ts->left = ts_insert(ts->left, val);
    } else if (val > ts->val) {
        ts->right = ts_insert(ts->right, val);
    }
    /* Intentionally do nothing if val == ts->val */
    return ts;
}

uint8_t ts_contains(tree_set_t *ts, uint64_t val) {
    if (ts == NULL) {
        return 0;
    } else if (val == ts->val) {
        return 1;
    } else if (val < ts->val) {
        return ts_contains(ts->left, val);
    } else {
        return ts_contains(ts->right, val);
    }
}

void ts_free(tree_set_t *ts) {
    if (ts->left != NULL) {
        ts_free(ts->left);
        ts->left = NULL;
    }
    if (ts->right != NULL) {
        ts_free(ts->right);
        ts->right = NULL;
    }
    free(ts);
    ts = NULL;
}
