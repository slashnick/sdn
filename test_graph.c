#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "graph.h"

static void print_visit(int sw, uint32_t port, void *host) {
    char *host_name = (char *)host;

    printf("S%d -> %s: port %d\n", sw, host_name, port);
}

int main(void) {
    graph_t *graph;
    int sw;
    size_t edge;
    vertex_sw_t *vertex;

    graph = init_graph();

    for (sw = 1; sw <= 5; sw++) {
        add_vertex_sw(graph, sw);
    }

    add_edge_sw(graph, 1, 1, 2, 1);
    add_edge_sw(graph, 1, 2, 4, 2);
    add_edge_sw(graph, 2, 2, 3, 3);
    add_edge_sw(graph, 2, 3, 4, 1);
    add_edge_sw(graph, 3, 2, 4, 3);
    add_edge_sw(graph, 4, 5, 5, 1);
    add_edge_sw(graph, 4, 5, 5, 1);

    for (sw = 0; (size_t)sw <= graph->max_vertex; sw++) {
        vertex = &graph->vertices_sw[sw];
        if (vertex->present) {
            printf("Have switch %d\n", sw);
            for (edge = 0; edge < vertex->num_edges; edge++) {
                printf("  %d -> %d (port %d)\n", sw, vertex->edges[edge].vertex,
                       vertex->edges[edge].my_port);
            }
        }
    }

    /* Walk the shortest path on the graph */
    walk_shortest_path(graph, 1, 3, "H1", print_visit);
    walk_shortest_path(graph, 2, 4, "H2", print_visit);
    walk_shortest_path(graph, 3, 1, "H3", print_visit);
    walk_shortest_path(graph, 4, 4, "H4", print_visit);
    walk_shortest_path(graph, 5, 2, "H5", print_visit);

    /* Free our shit */
    for (sw = 0; (size_t)sw <= graph->max_vertex; sw++) {
        if (graph->vertices_sw[sw].present) {
            free(graph->vertices_sw[sw].edges);
        }
    }
    free(graph->vertices_sw);
    free(graph);

    return 0;
}
