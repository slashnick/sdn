#ifndef GRAPH_H_
#define GRAPH_H_

#include <stddef.h>
#include <stdint.h>

/* An edge from one switch to another switch */
typedef struct {
    int vertex;
    uint32_t my_port;
    uint32_t their_port;
} edge_sw_t;

/* A switch vertex in the graph */
typedef struct {
    edge_sw_t *edges;
    size_t size_edges; /* Number of edge_sw_t's that edges has space for */
    size_t num_edges;
    uint8_t present;
} vertex_sw_t;

typedef struct {
    vertex_sw_t *vertices_sw; /* Array of switches */
    size_t max_vertex;
} graph_t;

typedef void(shortest_path_cb)(int, uint32_t, void *);

graph_t *init_graph(void);
void add_vertex_sw(graph_t *, int);
void add_edge_sw(graph_t *, int, uint32_t, int, uint32_t);
void walk_shortest_path(graph_t *, int, uint32_t, void *, uint8_t,
                        shortest_path_cb);

#endif /* GRAPH_H_ */
