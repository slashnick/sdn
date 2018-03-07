#ifndef GRAPH_H_
#define GRAPH_H_

#include <stdint.h>
#include <map>
#include <set>

typedef void(shortest_path_cb)(uint64_t, uint32_t, void *);
typedef std::map<uint32_t, std::pair<uint64_t, uint32_t> > edges_t;
typedef std::map<uint64_t, std::set<uint32_t> > MST;

class Graph {
   public:
    void add_vertex(uint64_t);
    void add_edge(uint64_t, uint32_t, uint64_t, uint32_t);
    void remove_edge(uint64_t, uint32_t);
    uint8_t has_edge(uint64_t, uint32_t, uint64_t, uint32_t) const;
    uint8_t has_any_edge(uint64_t, uint32_t) const;
    void walk_shortest_path(uint64_t, uint32_t, void *, uint8_t,
                            shortest_path_cb) const;
    MST *make_mst() const;
    std::map<uint64_t, edges_t> vertices;

   private:
    void add_single_edge(uint64_t, uint32_t, uint64_t, uint32_t);
    edges_t *get_or_add_vertex(uint64_t);
};

#endif /* GRAPH_H_ */
