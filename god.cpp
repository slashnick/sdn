#include "god.h"
#include <cstdio>
#include <set>
#include "client.h"
#include "event.h"
#include "graph.h"
#include "openflow.h"

void add_non_switch_ports(Client*, Graph*, std::set<uint32_t>*);

void god_function(Server* server) {
    Graph* graph = &server->graph;

    // MST
    static MST* old_mst = nullptr;
    MST* mst = graph->make_mst();
    MST::iterator vit;
    for (vit = mst->begin(); vit != mst->end(); vit++) {
        std::set<uint32_t>::const_iterator pit;
        if (old_mst == nullptr || (*old_mst)[vit->first] != vit->second) {
            Client* client = client_table[vit->first];
            add_non_switch_ports(client, graph, &vit->second);
            if (!client->has_mst) {
                if (vit->second.size() > 0) {
                    client->has_mst = 1;
                    update_broadcast_group(client, &vit->second, OFPGC_ADD);
                    add_broadcast_rule(client);
                }
            } else {
                update_broadcast_group(client, &vit->second, OFPGC_MODIFY);
            }
        }
    }
    if (old_mst != nullptr) {
        delete old_mst;
    }
    old_mst = mst;
}

void add_non_switch_ports(Client* client, Graph* graph, std::set<uint32_t>*
ports) {
    std::set<uint32_t>::const_iterator it;
    for (it = client->ports.begin(); it != client->ports.end(); it++) {
        if (!graph->has_any_edge(client->uid, *it)) {
            ports->insert(*it);
        }
    }
}
