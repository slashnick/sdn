#include "graph.h"
#include <stdint.h>
#include <map>
#include <queue>
#include <set>

typedef std::pair<uint32_t, std::pair<uint64_t, uint32_t> > edge_t;
typedef std::map<uint64_t, edges_t> vertices_t;

static void add_mst_edge(uint64_t, uint32_t, void*);

void Graph::add_single_edge(uint64_t from, uint32_t fport, uint64_t to,
                            uint32_t tport) {
    edges_t from_e = vertices[from];
    edges_t::iterator from_search = from_e.find(fport);
    std::pair<uint64_t, uint32_t> topair =
        std::pair<uint64_t, uint32_t>(to, tport);

    if (from_search != from_e.end()) {
        std::pair<uint64_t, uint32_t> existing_to = from_search->second;
        if (existing_to != topair) {
            // Remove the existing to-vertex's edge back to from
            vertices[existing_to.first].erase(existing_to.second);
            from_e.erase(from_search);
            from_e.insert(edge_t(fport, topair));
        }
    } else {
        from_e.insert(edge_t(fport, topair));
    }
}

void Graph::remove_edge(uint64_t from, uint32_t fport) {
    edges_t from_e = vertices[from];
    edges_t::iterator from_search = from_e.find(fport);

    if (from_search != from_e.end()) {
        std::pair<uint64_t, uint32_t> existing_to = from_search->second;
        vertices[existing_to.first].erase(existing_to.second);
        from_e.erase(from_search);
    }
}

void Graph::add_vertex(uint64_t id) {
    vertices[id];
}

void Graph::add_edge(uint64_t from, uint32_t fport, uint64_t to,
                     uint32_t tport) {
    add_single_edge(from, fport, to, tport);
    add_single_edge(to, tport, from, fport);
}

// Does this vertex have a specific switch & port connected to this port?
uint8_t Graph::has_edge(uint64_t from, uint32_t fport, uint64_t to,
                        uint32_t tport) const {
    vertices_t::const_iterator vsearch = vertices.find(from);
    if (vsearch == vertices.end()) {
        return 0;
    }
    edges_t::const_iterator esearch = vsearch->second.find(fport);
    if (esearch == vsearch->second.end()) {
        return 0;
    }
    return esearch->second.first == to && esearch->second.second == tport;
}

// Does this vertex have any switch connected to this port?
uint8_t Graph::has_any_edge(uint64_t from, uint32_t fport) const {
    vertices_t::const_iterator vsearch = vertices.find(from);
    if (vsearch == vertices.end()) {
        return 0;
    }

    edges_t edges = vsearch->second;
    return edges.find(fport) != edges.end();
}

void Graph::walk_shortest_path(uint64_t start, uint32_t start_port, void* data,
                               uint8_t bidirectional,
                               shortest_path_cb cb) const {
    std::queue<std::pair<uint64_t, uint32_t> > queue;
    std::set<uint64_t> visited;

    queue.push(std::pair<uint64_t, uint32_t>(start, start_port));
    visited.insert(start);

    while (!queue.empty()) {
        std::pair<uint64_t, uint32_t> vertex = queue.front();
        queue.pop();
        cb(vertex.first, vertex.second, data);

        edges_t edges = vertices.find(vertex.first)->second;
        for (edges_t::iterator it = edges.begin(); it != edges.end(); it++) {
            uint32_t my_port = it->first;
            uint64_t them = it->second.first;
            uint32_t their_port = it->second.second;
            if (visited.find(them) == visited.end()) {
                visited.insert(them);
                if (bidirectional) {
                    cb(vertex.first, my_port, data);
                }
                queue.push(std::pair<uint64_t, uint32_t>(them, their_port));
            }
        }
    }
}

#define IGNORE_EDGE 0xffffffff

void add_mst_edge(uint64_t vertex, uint32_t port, void* mst) {
    MST* m = reinterpret_cast<MST*>(mst);

    if (port == IGNORE_EDGE) {
        // The "start port" for an MST is meaningless to us
        return;
    }
    (*m)[vertex].insert(port);
}

MST* Graph::make_mst() const {
    MST* mst = new MST;

    vertices_t::const_iterator it = vertices.begin();
    if (it == vertices.end()) {
        return mst;
    }

    walk_shortest_path(it->first, IGNORE_EDGE, (void*)mst, 1, add_mst_edge);
    return mst;
}
