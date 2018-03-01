#include "mst.h"
#include <stdio.h>
#include <stdlib.h>

port_list_t **init_mst(size_t max_edge) {
    port_list_t **mst;

    if ((mst = calloc(max_edge + 1, sizeof(port_list_t *))) == NULL) {
        perror("malloc");
        exit(-1);
    }

    return mst;
}

void add_mst_edge(int vertex, uint32_t port, void *data) {
    port_list_t *node, **mst;

    mst = (port_list_t **)data;
    if (port == IGNORE_LINK) {
        /* For MST, we don't care about the "starting" port */
        return;
    }
    if ((node = malloc(sizeof(port_list_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
    node->next = mst[vertex];
    node->port = port;
    mst[vertex] = node;
}

void free_mst(port_list_t **mst, size_t size) {
    port_list_t *node;
    size_t fd;

    for (fd = 0; fd <= size; fd++) {
        while (mst[fd] != NULL) {
            node = mst[fd];
            mst[fd] = mst[fd]->next;
            free(node);
        }
    }
}
