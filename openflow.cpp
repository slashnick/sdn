#include "openflow.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <cstdio>
#include <set>
#include "beacon.h"
#include "event.h"
#include "god.h"
#include "graph.h"

std::map<uint64_t, Client *> client_table;
static uint8_t setup_done = 0;

enum ofp_type {
    OFPT_HELLO = 0,
    OFPT_ERROR = 1,
    OFPT_ECHO_REQ = 2,
    OFPT_ECHO_RES = 3,
    OFPT_FEATURE_REQ = 5,
    OFPT_FEATURE_RES = 6,
    OFPT_PACKET_IN = 10,
    OFPT_PORT_STATUS = 12,
    OFPT_PACKET_OUT = 13,
    OFPT_FLOW_MOD = 14,
    OFPT_GROUP_MOD = 15,
    OFPT_MULTIPART_REQ = 18,
    OFPT_MULTIPART_RES = 19
};

enum port_reason { PORT_ADD = 0, PORT_DEL = 1, PORT_MOD = 2 };

enum ofp_oxm_class { OFPXMC_OPENFLOW_BASIC = 0x8000 };

enum oxm_ofb_match_fields {
    OFPXMT_OFB_IN_PORT = 0,
    OFPXMT_OFB_ETH_DST = 3,
    OFPXMT_OFB_ETH_SRC = 4
};

/* Can't make values larger than a signed int in ISO C */
#define BCAST_GROUP_ID 0x7f5d8dda

#define OFPP_MAX 0xffffff00
//#define OFPP_ALL 0xfffffffc
#define OFPP_CONTROLLER 0xfffffffd
#define OFPP_ANY 0xffffffff

#define OFP_NO_BUFFER 0xffffffff

enum match_type { OFPMT_OXM = 1 };

enum instr_write_type { OFPIT_GOTO_TABLE = 1, OFPIT_WRITE_ACTIONS = 3 };

enum action_type { OFPAT_OUTPUT = 0, OFPAT_GROUP = 22 };

enum multipart_type { OFPMP_PORT_DESC = 13 };

typedef struct {
    uint16_t type;
    uint16_t code;
} __attribute__((packed)) ofp_error_t;

typedef struct {
    uint32_t datapath_id1;
    uint32_t datapath_id2;
    uint32_t n_buffers;
    uint8_t n_tables;
    uint8_t auxiliary_id;
    uint8_t _pad[2];
    uint32_t capabilities;
    uint32_t actions;
} __attribute__((packed)) switch_features_t;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint8_t oxm_fields[4];
} __attribute__((packed)) match_t;

typedef struct {
    uint64_t cookie;
    uint64_t cookie_mask;
    uint8_t table_id;
    uint8_t command;
    uint16_t idle_timeout;
    uint16_t hard_timeout;
    uint16_t priority;
    uint32_t buffer_id;
    uint32_t out_port;
    uint32_t out_group;
    uint16_t flags;
    uint8_t _pad[2];
    match_t match[];
} __attribute__((packed)) flow_mod_t;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint32_t port;
    uint16_t max_len;
    uint8_t pad[6];
} __attribute__((packed)) action_output_t;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint32_t group_id;
} __attribute__((packed)) action_group_t;

typedef struct {
    uint16_t len;
    uint16_t weight;
    uint32_t watch_port;
    uint32_t watch_group;
    uint8_t _pad[4];
} __attribute__((packed)) bucket_t;

typedef struct {
    uint16_t command;
    uint8_t type;
    uint8_t _pad;
    uint32_t group_id;
    bucket_t buckets[];
} __attribute__((packed)) group_mod_t;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint8_t _pad[4];
    action_output_t actions[];
} __attribute__((packed)) instr_write_t;

typedef struct {
    uint16_t type;
    uint16_t length;
    uint8_t table_id;
    uint8_t _pad[3];
} __attribute__((packed)) instr_goto_t;

typedef struct {
    uint32_t buffer_id;
    uint16_t total_len;
    uint8_t reason;
    uint8_t tbl_id;
    uint64_t cookie;
    match_t match;
} __attribute__((packed)) packet_in_t;

typedef struct {
    uint32_t buffer_id;
    uint32_t in_port;
    uint16_t actions_len;
    uint8_t _pad[6];
    action_output_t actions[];
} __attribute__((packed)) packet_out_t;

typedef struct {
    uint16_t type;
    uint16_t flags;
    uint8_t _pad[4];
    uint8_t body[];
} __attribute__((packed)) multipart_t;

typedef struct {
    uint32_t port_id;
    uint8_t _pad[4];
    uint8_t hw_addr[6];
    uint8_t _pad2[2];
    char name[16];
    uint32_t config;
    uint32_t state;
    uint32_t curr;
    uint32_t advertised;
    uint32_t supported;
    uint32_t peer;
    uint32_t cur_speed;
    uint32_t max_speed;
} __attribute__((packed)) port_t;

typedef struct {
    uint8_t reason;
    uint8_t _pad[7];
    port_t port;
} __attribute__((packed)) port_status_t;

static ofp_header_t *make_packet(uint8_t, uint16_t, uint32_t);
static void setup_table_miss(Client *);
static void add_unicast_rule(uint64_t, uint32_t, void *);
static void add_source_mac_rule(Client *, const void *);
static void handle_hello(Client *);
static void handle_error(Client *);
static void handle_feature_res(Client *);
static void handle_multipart_res(Client *);
static void handle_echo_req(Client *);
static void handle_packet_in(Client *);
static void handle_port_status(Client *);

void init_connection(Client *client) {
    ofp_header_t *hello;

    hello = make_packet(OFPT_HELLO, sizeof(ofp_header_t), 666);
    client->write_packet(hello, sizeof(ofp_header_t));
    add_dest_mac_rule(client, SWITCH_POLL_MAGIC, OFPP_CONTROLLER, FM_CMD_ADD,
                      0);
    setup_table_miss(client);
}

void setup_table_miss(Client *client) {
    ofp_header_t *pack;
    flow_mod_t *flow_mod;
    match_t *match;
    instr_write_t *instr1;
    instr_goto_t *instr2;
    action_output_t *action;
    uint16_t length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                      sizeof(match_t) + sizeof(instr_write_t) +
                      sizeof(action_output_t) + sizeof(instr_goto_t);

    pack = make_packet(OFPT_FLOW_MOD, length, 0x1234321);
    flow_mod = (flow_mod_t *)pack->data;
    match = flow_mod->match;
    instr2 = (instr_goto_t *)((uint8_t *)match + sizeof(match_t));
    instr1 = (instr_write_t *)(instr2 + 1);
    action = instr1->actions;

    /* Add a rule */
    flow_mod->table_id = 0;
    flow_mod->command = FM_CMD_ADD;
    flow_mod->buffer_id = htonl(OFP_NO_BUFFER);
    flow_mod->out_port = OFPP_ANY;
    flow_mod->out_group = OFPP_ANY;

    match->type = htons(OFPMT_OXM);
    match->length = htons(4);

    instr1->type = htons(OFPIT_WRITE_ACTIONS);
    instr1->length = htons(sizeof(instr_write_t) + sizeof(action_output_t));

    action->type = htons(OFPAT_OUTPUT);
    action->length = htons(sizeof(action_output_t));
    action->port = htonl(OFPP_CONTROLLER);
    action->max_len = htons(0xffff);

    instr2->type = htons(OFPIT_GOTO_TABLE);
    instr2->length = htons(sizeof(instr_goto_t));
    instr2->table_id = 1;

    client->write_packet(pack, length);
}

void update_broadcast_group(Client *client, const std::set<uint32_t> *ports,
                            uint16_t cmd) {
    uint16_t packet_length =
        sizeof(ofp_header_t) + sizeof(group_mod_t) +
        (uint16_t)ports->size() * (sizeof(bucket_t) + sizeof(action_output_t));
    ofp_header_t *pack = make_packet(OFPT_GROUP_MOD, packet_length, 0);
    group_mod_t *group_mod = (group_mod_t *)pack->data;

    group_mod->command = htons(cmd);
    group_mod->group_id = htonl(BCAST_GROUP_ID);

    bucket_t *bucket = group_mod->buckets;
    std::set<uint32_t>::const_iterator it;
    for (it = ports->begin(); it != ports->end(); it++) {
        bucket->len = htons(sizeof(bucket_t) + sizeof(action_output_t));
        bucket->watch_port = htonl(OFPP_ANY);
        bucket->watch_group = htonl(OFPP_ANY);

        action_output_t *action = (action_output_t *)(bucket + 1);
        action->type = htons(OFPAT_OUTPUT);
        action->length = htons(sizeof(action_output_t));
        action->port = htonl(*it);
        action->max_len = htons(0xffff);

        bucket = (bucket_t *)(action + 1);
    }

    client->write_packet(pack, packet_length);
}

void add_broadcast_rule(Client *client) {
    uint16_t packet_length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                             sizeof(match_t) + sizeof(instr_write_t) +
                             sizeof(action_group_t);
    ofp_header_t *pack = make_packet(OFPT_FLOW_MOD, packet_length, 7);

    flow_mod_t *flow_mod = (flow_mod_t *)pack->data;
    flow_mod->table_id = 1;
    flow_mod->command = FM_CMD_ADD;
    flow_mod->buffer_id = htonl(OFP_NO_BUFFER);
    flow_mod->priority = htons(1);
    flow_mod->out_port = OFPP_ANY;
    flow_mod->out_group = OFPP_ANY;

    match_t *match = flow_mod->match;
    match->type = htons(OFPMT_OXM);
    match->length = htons(4);

    instr_write_t *instr = (instr_write_t *)(match + 1);
    instr->type = htons(OFPIT_WRITE_ACTIONS);
    instr->length = htons(sizeof(instr_write_t) + sizeof(action_group_t));

    action_group_t *action_g = (action_group_t *)instr->actions;
    action_g->type = htons(OFPAT_GROUP);
    action_g->length = htons(sizeof(action_group_t));
    action_g->group_id = htonl(BCAST_GROUP_ID);

    client->write_packet(pack, packet_length);
}

void add_unicast_rule(uint64_t uid, uint32_t port, void *mac) {
    Client *client = client_table[uid];
    add_source_mac_rule(client, mac);
    add_dest_mac_rule(client, mac, port, FM_CMD_ADD, 1);
}

void add_source_mac_rule(Client *client, const void *mac) {
    ofp_header_t *pack;
    flow_mod_t *flow_mod;
    match_t *match;
    instr_goto_t *instr;
    uint32_t *fields;
    uint16_t length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                      sizeof(match_t) + 8 + sizeof(instr_goto_t);

    pack = make_packet(OFPT_FLOW_MOD, length, 0);
    flow_mod = (flow_mod_t *)(pack->data);
    match = flow_mod->match;
    instr = (instr_goto_t *)((uint8_t *)match + sizeof(match_t) + 8);

    flow_mod->table_id = 0;
    flow_mod->command = FM_CMD_ADD;
    flow_mod->buffer_id = htonl(OFP_NO_BUFFER);
    flow_mod->priority = htons(2);
    flow_mod->out_port = OFPP_ANY;
    flow_mod->out_group = OFPP_ANY;

    match->type = htons(OFPMT_OXM);
    match->length = htons(14);
    fields = (uint32_t *)match->oxm_fields;
    fields[0] = htonl(((uint32_t)OFPXMC_OPENFLOW_BASIC << 16) |
                      (OFPXMT_OFB_ETH_SRC << 9) | 6);
    memcpy(fields + 1, mac, 6);

    instr->type = htons(OFPIT_GOTO_TABLE);
    instr->length = htons(sizeof(instr_goto_t));
    instr->table_id = 1;

    client->write_packet(pack, length);
}

void add_dest_mac_rule(Client *client, const void *mac, uint32_t port_id,
                       uint8_t cmd, uint8_t table_id) {
    ofp_header_t *pack;
    flow_mod_t *flow_mod;
    match_t *match;
    instr_write_t *instr;
    action_output_t *action;
    uint32_t *fields;
    uint16_t length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                      sizeof(match_t) + 8 + sizeof(instr_write_t) +
                      sizeof(action_output_t);

    pack = make_packet(OFPT_FLOW_MOD, length, 0);
    flow_mod = (flow_mod_t *)pack->data;
    match = flow_mod->match;
    instr = (instr_write_t *)((uint8_t *)match + sizeof(match_t) + 8);
    action = instr->actions;

    /* Add a rule */
    flow_mod->table_id = table_id;
    flow_mod->command = cmd;
    flow_mod->buffer_id = htonl(OFP_NO_BUFFER);
    flow_mod->priority = htons(11);
    flow_mod->out_port = OFPP_ANY;
    flow_mod->out_group = OFPP_ANY;

    match->type = htons(OFPMT_OXM);
    match->length = htons(14);
    fields = (uint32_t *)match->oxm_fields;
    fields[0] = htonl(((uint32_t)OFPXMC_OPENFLOW_BASIC << 16) |
                      (OFPXMT_OFB_ETH_DST << 9) | 6);
    memcpy(fields + 1, mac, 6);

    instr->type = htons(OFPIT_WRITE_ACTIONS);
    instr->length = htons(sizeof(instr_write_t) + sizeof(action_output_t));

    action->type = htons(OFPAT_OUTPUT);
    action->length = htons(sizeof(action_output_t));
    action->port = htonl(port_id);
    action->max_len = htons(0xffff);

    client->write_packet(pack, length);
}

void handle_ofp_packet(Client *client) {
    switch (client->cur_packet->type) {
        case OFPT_HELLO:
            handle_hello(client);
            break;
        case OFPT_ERROR:
            handle_error(client);
            break;
        case OFPT_FEATURE_RES:
            handle_feature_res(client);
            break;
        case OFPT_MULTIPART_RES:
            handle_multipart_res(client);
            break;
        case OFPT_ECHO_REQ:
            handle_echo_req(client);
            break;
        case OFPT_PACKET_IN:
            handle_packet_in(client);
            break;
        case OFPT_PORT_STATUS:
            handle_port_status(client);
            break;
        default:
            fprintf(stderr, "Got unexpected packet type 0x%02x\n",
                    client->cur_packet->type);
            break;
    }
}

/* Caller is responsible for freeing this pointer */
ofp_header_t *make_packet(uint8_t type, uint16_t length, uint32_t xid) {
    ofp_header_t *hdr;

    if ((hdr = (ofp_header_t *)calloc(1, length)) == nullptr) {
        perror("calloc");
        exit(-1);
    }

    hdr->version = 0x04;
    hdr->type = type;
    hdr->length = htons(length);
    hdr->xid = xid;

    return hdr;
}

void handle_hello(Client *client) {
    ofp_header_t *res;

    res = make_packet(OFPT_FEATURE_REQ, sizeof(ofp_header_t), 777);
    client->write_packet(res, sizeof(ofp_header_t));
}

void handle_error(Client *client) {
    const ofp_error_t *err;

    err = (ofp_error_t *)client->cur_packet->data;

    if (ntohs(err->type) == 1) {
        switch (ntohs(err->code)) {
            case 0x0002:
                printf("Error: bad multipart\n");
                break;
            case 0x0006:
                printf("Error: bad length\n");
                break;
            default:
                printf("Error: bad request (code=0x%04x)\n", ntohs(err->code));
        }
    } else {
        printf("Error: type=0x%04x code=0x%04x\n", ntohs(err->type),
               ntohs(err->code));
    }
}

void handle_feature_res(Client *client) {
    const switch_features_t *features;
    ofp_header_t *mp_pack;
    multipart_t *req;

    if (client->cur_packet->length < sizeof(switch_features_t)) {
        fprintf(stderr, "Feature packet too short\n");
        return;
    }
    features = (switch_features_t *)client->cur_packet->data;

    client->uid = ((uint64_t)ntohl(features->datapath_id1) << 32) |
                  ntohl(features->datapath_id2);
    // Now that we have a client UID, add the client to the graph
    Graph *graph = &((Server *)client->server)->graph;
    graph->add_vertex(client->uid);
    client_table[client->uid] = client;

    // Send a multipart port stats request
    mp_pack = make_packet(OFPT_MULTIPART_REQ,
                          sizeof(ofp_header_t) + sizeof(multipart_t), 888);
    req = (multipart_t *)mp_pack->data;
    memset(req, 0, sizeof(multipart_t));
    req->type = htons(OFPMP_PORT_DESC);
    client->write_packet(mp_pack, sizeof(ofp_header_t) + sizeof(multipart_t));
}

void handle_multipart_res(Client *client) {
    multipart_t *mp;
    port_t *ports;
    size_t ndx, num_ports;
    char port_name[16];
    uint32_t port_id;

    mp = (multipart_t *)client->cur_packet->data;
    if (ntohs(mp->type) != OFPMP_PORT_DESC) {
        return;
    }

    ports = (port_t *)mp->body;
    num_ports = (client->cur_packet->length - sizeof(ofp_header_t) -
                 sizeof(multipart_t)) /
                sizeof(port_t);
    for (ndx = 0; ndx < num_ports; ndx++) {
        snprintf(port_name, sizeof(port_name), "%s", ports[ndx].name);
        port_id = ntohl(ports[ndx].port_id);
        if (port_id <= OFPP_MAX) {
            client->ports.insert(port_id);
        }
    }
    send_polls(client);
}

void handle_echo_req(Client *client) {
    const ofp_header_t *req;
    ofp_header_t *res;
    uint16_t length;

    req = client->cur_packet;
    /* We guarantee that a processed packet will never have
     * length < sizeof(ofp_header_t) */
    length = req->length;

    res = make_packet(OFPT_ECHO_RES, length, client->cur_packet->xid);
    memcpy(res->data, req->data, length - sizeof(ofp_header_t));

    client->write_packet(res, length);
}

void handle_packet_in(Client *client) {
    if (client->cur_packet->length <
        sizeof(ofp_header_t) + sizeof(packet_in_t)) {
        fprintf(stderr, "packet_in too short\n");
        return;
    }
    packet_in_t *pack = (packet_in_t *)client->cur_packet->data;
    match_t *match = &pack->match;
    uint8_t *data =
        (uint8_t *)&pack->match + (ntohs(pack->match.length) + 7) / 8 * 8 + 2;

    uint32_t *fields = (uint32_t *)match->oxm_fields;
    /* We only care about PACKET_IN's that have the in port */
    if (((htonl(fields[0]) >> 9) & 0x7f) != OFPXMT_OFB_IN_PORT) {
        return;
    }

    uint32_t port_id = ntohl(fields[1]);
    if (!memcmp(data, SWITCH_POLL_MAGIC, 6)) {
        recv_poll(client, port_id, data);
        return;
    }

    Server *server = (Server *)client->server;
    Graph *graph = &server->graph;
    if (graph->has_any_edge(client->uid, port_id)) {
        // Skip non-beacon PACKET_IN's from other switches
        return;
    }

    /* Put the mac address in the lower 6 bytes of mac */
    uint64_t mac = ((uint64_t)data[6] << 40) | ((uint64_t)data[7] << 32) |
                   ((uint64_t)data[8] << 24) | ((uint64_t)data[9] << 16) |
                   ((uint64_t)data[10] << 8) | data[11];

    if (client->hosts[mac] != port_id) {
        client->hosts[mac] = port_id;
        god_function(server);
    }
}

void send_packet_out(Client *client, uint32_t port, const void *data,
                     uint16_t length) {
    ofp_header_t *hdr;
    packet_out_t *pack;
    action_output_t *action;
    uint16_t total_length;

    total_length = sizeof(ofp_header_t) + sizeof(packet_out_t) +
                   sizeof(action_output_t) + length;
    hdr = make_packet(OFPT_PACKET_OUT, total_length, 0);
    pack = (packet_out_t *)hdr->data;
    action = pack->actions;

    pack->buffer_id = htonl(OFP_NO_BUFFER);
    pack->in_port = htonl(OFPP_CONTROLLER);
    pack->actions_len = htons(sizeof(action_output_t));

    action->type = htons(OFPAT_OUTPUT);
    action->length = htons(sizeof(action_output_t));
    action->port = htonl(port);
    action->max_len = htons(0xffff);

    memcpy((uint8_t *)action + sizeof(action_output_t), data, length);

    client->write_packet(hdr, total_length);
}

void handle_port_status(Client *client) {
    const port_status_t *pack;

    if (client->cur_packet->length <
        sizeof(ofp_header_t) + sizeof(port_status_t)) {
        fprintf(stderr, "port_status too short\n");
        return;
    }
    pack = (port_status_t *)client->cur_packet->data;

    // TODO: this is like probably fine, we shouldn't poll
    switch (pack->reason) {
        case PORT_MOD:
            if (htonl(pack->port.port_id) <= OFPP_MAX) {
                // send_poll(client, ntohl(pack->port.port_id), 1000);
            }
            break;
    }
}
