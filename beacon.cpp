/* Switch-to-switch link discovery */

#include "beacon.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <set>
#include "event.h"
#include "openflow.h"

// Set of outstanding polls, so we don't fire more than one event per poll
typedef std::map<uint32_t, std::pair<uint64_t, uint32_t> > outstanding_t;
static outstanding_t outstanding;

static void send_poll(Client*, uint32_t, uint64_t);
static void poll_timeout(void*);

void send_polls(void* cptr) {
    Client* client = (Client*)cptr;
    Server* server = (Server*)client->server;
    std::set<uint32_t>::iterator it;
    for (it = client->ports.begin(); it != client->ports.end(); it++) {
        send_poll(client, *it, 750);
    }
    server->schedule_event(1000, send_polls, cptr);
}

void send_poll(Client* client, uint32_t port, uint64_t timeout) {
    static uint64_t poll_id = 0;
    Server* server = (Server*)client->server;

    poll_id++;
    outstanding.insert(std::pair<uint32_t, std::pair<uint64_t, uint32_t> >(
        (uint32_t)poll_id, std::pair<uint64_t, uint32_t>(client->uid, port)));

    switch_poll_t beacon;
    memcpy(beacon.magic, SWITCH_POLL_MAGIC, 6);
    beacon.poll_id = htonl((uint32_t)poll_id);
    beacon.uid1 = htonl(client->uid >> 32);
    beacon.uid1 = htonl((uint32_t)client->uid);
    beacon.port_id = htonl(port);

    send_packet_out(client, port, &beacon, sizeof(beacon));
    server->schedule_event(timeout, poll_timeout, (void*)poll_id);
}

void recv_poll(Client* client, uint32_t port, const uint8_t* data) {
    Graph* graph = &((Server*)client->server)->graph;
    switch_poll_t beacon;
    memcpy(&beacon, data, sizeof(beacon));

    uint32_t poll_id = ntohl(beacon.poll_id);
    uint64_t from_uid =
        ((uint64_t)ntohl(beacon.uid1) << 32) | ntohl(beacon.uid2);
    uint32_t from_port = ntohl(beacon.port_id);

    if (outstanding.erase(poll_id) == 0) {
        // Poll has already been handled (timeout fired or duplicate frame)
        return;
    } else if (graph->has_edge(from_uid, from_port, client->uid, port)) {
        // Edge exists, do nothing
        return;
    }
    graph->add_edge(from_uid, from_port, client->uid, port);
    // TODO: MAIN LOGIC, RECOMPUTE THE GRAPH GOGOGO
}

void poll_timeout(void* arg) {
    uint64_t poll_id = (uint64_t)arg;
    outstanding_t::iterator it = outstanding.find((uint32_t)poll_id);
    if (it == outstanding.end()) {
        // Poll has already been handled (timeout fired or duplicate frame)
        return;
    }
    uint64_t uid = it->second.first;
    uint32_t port = it->second.second;
    outstanding.erase(it);

    Client* client = client_table[uid];
    Server* server = (Server*)client->server;
    Graph* graph = &server->graph;
    if (!graph->has_any_edge(uid, port)) {
        // Edge wasn't there to begin with, do nothing
        return;
    }
    graph->remove_edge(uid, port);
    // TODO: MAIN LOGIC, RECOMPUTE THE GRAPH GOGOGO
}
