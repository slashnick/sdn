#include "graph.h"
#include <stdint.h>
#include <map>
#include <queue>
#include <set>

typedef std::pair<uint64_t, std::pair<uint32_t, uint32_t> > edge_t;
typedef std::map<uint64_t, edges_t*> vertices_t;

static void add_mst_edge(uint64_t, uint32_t, void*);

void Graph::add_single_edge(uint64_t from, uint32_t fport, uint64_t to,
                            uint32_t tport) {
    edges_t* from_e = get_or_add_vertex(from);

    edges_t::iterator from_search = from_e->find(to);

    if (from_search == from_e->end()) {
        from_e->insert(edge_t(to, std::pair<uint32_t, uint32_t>(fport, tport)));
    }
}

edges_t* Graph::get_or_add_vertex(uint64_t id) {
    vertices_t::iterator search = vertices.find(id);
    if (search == vertices.end()) {
        edges_t* edges = new edges_t;
        vertices.insert(std::pair<uint64_t, edges_t*>(id, edges));
        return edges;
    } else {
        return search->second;
    }
}

void Graph::add_vertex(uint64_t id) {
    get_or_add_vertex(id);
}

void Graph::add_edge(uint64_t from, uint32_t fport, uint64_t to,
                     uint32_t tport) {
    add_single_edge(from, fport, to, tport);
    add_single_edge(to, tport, from, fport);
}

uint8_t Graph::has_edge(uint64_t from, uint64_t to) const {
    vertices_t::const_iterator vsearch = vertices.find(from);
    if (vsearch == vertices.end()) {
        return 0;
    }
    const edges_t* edges = vsearch->second;
    return edges->find(to) != edges->end();
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

        const edges_t* edges = vertices.find(vertex.first)->second;
        edges_t::const_iterator it;
        for (it = edges->begin(); it != edges->end(); it++) {
            uint64_t them = it->first;
            uint32_t my_port = it->second.first;
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
