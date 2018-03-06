#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../graph.h"

static void print_visit(int sw, uint32_t port, void *host) {
    const char *host_name = (const char *)host;

    printf("S%d -> %s: port %d\n", sw, host_name, port);
}

static void mst_visit(int sw, uint32_t port, void *_ignore) {
    if (port == 0xffffffff) {
        return;
    }
    printf("S%d p%d\n", sw, port);
}

int main(void) {
    void *graph;
    uint64_t sw, sw1, sw2;

    graph = graph_create();

    for (sw = 1; sw <= 5; sw++) {
        add_vertex(graph, sw);
    }

    add_edge(graph, 1, 1, 2, 1);
    add_edge(graph, 1, 2, 4, 2);
    add_edge(graph, 2, 2, 3, 3);
    add_edge(graph, 2, 3, 4, 1);
    add_edge(graph, 3, 2, 4, 3);
    add_edge(graph, 4, 5, 5, 1);
    add_edge(graph, 4, 5, 5, 1);

    for (sw1 = 0; sw1 < 6; sw1++) {
        for (sw2 = 0; sw1 < 6; sw1++) {
            if (has_edge(graph, sw1, sw2)) {
                printf("  %lu -> %lu\n", sw1, sw2);
            }
        }
    }

    /* Walk the shortest path on the graph */
    walk_shortest_path(graph, 1, 3, (void *)"H1", 0, print_visit);
    walk_shortest_path(graph, 2, 4, (void *)"H2", 0, print_visit);
    walk_shortest_path(graph, 3, 1, (void *)"H3", 0, print_visit);
    walk_shortest_path(graph, 4, 4, (void *)"H4", 0, print_visit);
    walk_shortest_path(graph, 5, 2, (void *)"H5", 0, print_visit);

    /* Print the MST */
    walk_shortest_path(graph, 5, 0xffffffff, NULL, 1, mst_visit);

    graph_free(graph);

    return 0;
}
