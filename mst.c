#include "mst.h"

typedef struct node {
    struct node *next;
    uint32_t port;
} node_t;

static node_t *mst;

void start_mst(int max_edge) {
    if ((mst = calloc((size_t)max_edge * sizeof(node_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
}

void add_mst_edge(int vertex, uint32_t port, const void *_ignore) {
    if (port == IGNORE_LINK) {
        /* For MST, we don't care about the "starting" port */
        return;
    }
}
