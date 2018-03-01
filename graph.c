#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct bfs_node {
    struct bfs_node *next;
    int vertex;
    uint32_t port;
} bfs_node_t;

typedef struct {
    bfs_node_t *head;
    bfs_node_t *tail;
} bfs_queue_t;

static uint8_t vertex_has_edge(const vertex_sw_t *, int);
static void add_single_edge(graph_t *, int, uint32_t, int, uint32_t);
static void enqueue(bfs_queue_t *, int, uint32_t);
static bfs_node_t *dequeue(bfs_queue_t *);

graph_t *init_graph(void) {
    graph_t *graph;
    if ((graph = malloc(sizeof(graph_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
    graph->max_vertex = 127;
    graph->vertices_sw = calloc(graph->max_vertex + 1, sizeof(vertex_sw_t));
    if (graph->vertices_sw == NULL) {
        perror("malloc");
        exit(-1);
    }
    return graph;
}

void add_vertex_sw(graph_t *graph, int id) {
    size_t old_max_vertex;
    vertex_sw_t *vertex;

    /* Dynamically grow graph vertex array */
    if ((size_t)id > graph->max_vertex) {
        old_max_vertex = graph->max_vertex;
        while ((size_t)id > graph->max_vertex) {
            graph->max_vertex = graph->max_vertex * 2 + 1;
        }
        graph->vertices_sw = realloc(
            graph->vertices_sw, (graph->max_vertex + 1) * sizeof(vertex_sw_t));
        if (graph->vertices_sw == NULL) {
            perror("realloc");
            exit(-1);
        }
        /* Zero out the new memory */
        memset(&graph->vertices_sw[old_max_vertex + 1], 0,
               (graph->max_vertex - old_max_vertex) * sizeof(vertex_sw_t));
    }

    vertex = &graph->vertices_sw[id];
    vertex->present = 1;
    vertex->num_edges = 0;
    vertex->size_edges = 1;
    vertex->edges = malloc(vertex->size_edges * sizeof(edge_sw_t));
    if (vertex->edges == NULL) {
        perror("malloc");
        exit(-1);
    }
}

/* Add two edges, from vertex1->vertex2 and vertex2->vertex1
 *
 * port1 is the id of a physical port on vertex1
 * port2 is the id of a physical port on vertex2
 */
void add_edge_sw(graph_t *graph, int vertex1, uint32_t port1, int vertex2,
                 uint32_t port2) {
    add_single_edge(graph, vertex1, port1, vertex2, port2);
    add_single_edge(graph, vertex2, port2, vertex1, port1);
}

uint8_t vertex_has_edge(const vertex_sw_t *vertex, int to) {
    size_t edge_ndx;

    for (edge_ndx = 0; edge_ndx < vertex->num_edges; edge_ndx++) {
        if (vertex->edges[edge_ndx].vertex == to) {
            return 1;
        }
    }
    return 0;
}

/* my_port should be a physical port on me */
void add_single_edge(graph_t *graph, int me, uint32_t my_port, int them,
                     uint32_t their_port) {
    vertex_sw_t *vertex;
    edge_sw_t *edge;

    if ((size_t)me > graph->max_vertex || (size_t)them > graph->max_vertex) {
        fprintf(stderr, "Tried to add edge to invalid vertex\n");
        return;
    }

    vertex = &graph->vertices_sw[me];
    /* Don't create duplicate edges */
    if (vertex_has_edge(vertex, them)) {
        return;
    }

    vertex->num_edges++;
    /* Dynamically resize edge array */
    if (vertex->num_edges == vertex->size_edges) {
        vertex->size_edges *= 2;
        vertex->edges =
            realloc(vertex->edges, vertex->size_edges * sizeof(edge_sw_t));
        if (vertex->edges == NULL) {
            perror("realloc");
            exit(-1);
        }
    }

    edge = &vertex->edges[vertex->num_edges - 1];
    edge->vertex = them;
    edge->my_port = my_port;
    edge->their_port = their_port;
}

/* Perform a breadth-first walk down the graph, call cb for every node */
void walk_shortest_path(graph_t *graph, int start, uint32_t start_port,
                        void *data, uint8_t bidirectional,
                        shortest_path_cb cb) {
    uint8_t *visited;
    bfs_queue_t queue = {NULL, NULL};
    bfs_node_t *node;
    vertex_sw_t *vertex;
    edge_sw_t *edge;
    size_t edge_ndx;
    int vertex_id;
    uint32_t port;

    if ((visited = calloc(graph->max_vertex + 1, sizeof(uint8_t))) == NULL) {
        perror("calloc");
        exit(-1);
    }

    enqueue(&queue, start, start_port);
    visited[start] = 1;

    while ((node = dequeue(&queue))) {
        vertex_id = node->vertex;
        port = node->port;
        free(node);
        node = NULL;

        /* Call cb to visit the node */
        cb(vertex_id, port, data);

        vertex = &graph->vertices_sw[vertex_id];
        for (edge_ndx = 0; edge_ndx < vertex->num_edges; edge_ndx++) {
            edge = &vertex->edges[edge_ndx];
            if (!visited[edge->vertex]) {
                visited[edge->vertex] = 1;
                if (bidirectional) {
                    cb(vertex_id, edge->my_port, data);
                }
                enqueue(&queue, edge->vertex, edge->their_port);
            }
        }
    }

    free(visited);
    visited = NULL;
}

/* Allocated node belongs to queue */
void enqueue(bfs_queue_t *queue, int vertex, uint32_t port) {
    bfs_node_t *node;

    if ((node = malloc(sizeof(bfs_node_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
    node->vertex = vertex;
    node->port = port;
    node->next = NULL;

    if (queue->tail == NULL) {
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }
}

/* Caller is responsible for freeing node */
bfs_node_t *dequeue(bfs_queue_t *queue) {
    bfs_node_t *node;

    if (queue->head == NULL) {
        return NULL;
    }
    node = queue->head;
    queue->head = queue->head->next;
    if (queue->tail == node) {
        queue->tail = NULL;
    }
    return node;
}
