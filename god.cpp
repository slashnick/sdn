#include "god.h"
#include <cstdio>
#include <set>
#include "client.h"
#include "event.h"
#include "graph.h"
#include "openflow.h"

void god_mst(Server* server);
void update_host_sp(uint64_t, uint32_t, void*);
void add_non_switch_ports(Client*, Graph*, std::set<uint32_t>*);

// This runs on every dynamic link event
void god_function(Server* server) {
    god_mst(server);
    god_dijkstra(server);
}

void god_mst(Server* server) {
    Graph* graph = &server->graph;

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

void update_host_sp(uint64_t vertex, uint32_t port, void* ptr) {
    uint64_t mac = *(uint64_t*)ptr;
    uint8_t addr[6];
    addr[0] = mac & 0xff;
    addr[1] = (mac >> 8) & 0xff;
    addr[2] = (mac >> 16) & 0xff;
    addr[3] = (mac >> 24) & 0xff;
    addr[4] = (mac >> 32) & 0xff;
    addr[5] = (mac >> 40) & 0xff;

    Client* client = client_table[vertex];
    if (client->written.find(mac) == client->written.end()) {
        add_dest_mac_rule(client, addr, port, FM_CMD_ADD, 1);
    } else if (client->written[mac] != port) {
        add_dest_mac_rule(client, addr, port, FM_CMD_MODIFY, 1);
    }
}

void god_dijkstra(Server* server) {
    Graph* graph = &server->graph;

    std::map<uint64_t, edges_t>::iterator vit;
    for (vit = graph->vertices.begin(); vit != graph->vertices.end(); vit++) {
        Client* client = client_table[vit->first];
        std::map<uint64_t, uint32_t>::iterator hit;
        for (hit = client->hosts.begin(); hit != client->hosts.end(); hit++) {
            graph->walk_shortest_path(vit->first, hit->second,
                                      (void*)&hit->first, 0, update_host_sp);
        }
    }
}

// Insert all of `client`'s ports that don't go to a switch into `ports`
void add_non_switch_ports(Client* client, Graph* graph,
                          std::set<uint32_t>* ports) {
    std::set<uint32_t>::const_iterator it;
    for (it = client->ports.begin(); it != client->ports.end(); it++) {
        if (!graph->has_any_edge(client->uid, *it)) {
            ports->insert(*it);
        }
    }
}
