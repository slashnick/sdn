#include "openflow.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "graph.h"
#include "mst.h"
#include "tree_set.h"

client_t *of_clients = NULL;

static graph_t *graph = NULL;
static tree_set_t *seen_hosts = NULL;
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
    OFPT_MULTIPART_REQ = 18,
    OFPT_MULTIPART_RES = 19,
};

enum port_reason {
    PORT_ADD = 0,
    PORT_DEL = 1,
    PORT_MOD = 2,
};

enum flow_mod_cmd {
    FM_CMD_ADD = 0,
};

enum ofp_oxm_class {
    OFPXMC_OPENFLOW_BASIC = 0x8000,
};

enum oxm_ofb_match_fields {
    OFPXMT_OFB_IN_PORT = 0,
    OFPXMT_OFB_ETH_DST = 3,
    OFPXMT_OFB_ETH_SRC = 4,
};

/* Can't make values larger than a signed int in ISO C */
#define OFPP_MAX 0xffffff00
#define OFPP_ALL 0xfffffffc
#define OFPP_CONTROLLER 0xfffffffd
#define OFPP_ANY 0xffffffff

#define OFP_NO_BUFFER 0xffffffff

#define SWITCH_POLL_MAGIC 0xb18a91a3

enum match_type {
    OFPMT_OXM = 1,
};

enum instr_write_type {
    OFPIT_GOTO_TABLE = 1,
    OFPIT_WRITE_ACTIONS = 3,
};

enum action_type {
    OFPAT_OUTPUT = 0,
};

enum multipart_type {
    OFPMP_PORT_DESC = 13,
};

typedef struct {
    uint16_t type;
    uint16_t code;
} __attribute__((packed)) ofp_error_t;

typedef struct {
    uint8_t datapath_id[8];
    uint32_t n_buffers;
    uint8_t n_tables;
    uint8_t auxiliary_id;
    uint8_t _pad[2];
    uint32_t capabilities;
    uint32_t actions;
} __attribute__((packed)) switch_features_t;

typedef struct {
    uint32_t magic;
    int fd;
    uint32_t port_id;
    uint8_t _pad[4];
} __attribute__((packed)) switch_poll_t;

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
static void setup_table_miss(client_t *);
static void setup_broadcast(void);
static void add_broadcast_rule(int, const port_list_t *);
static void add_unicast_rule(int, uint32_t, void *);
static void add_source_mac_rule(client_t *, const void *);
static void add_dest_mac_rule(client_t *, const void *, uint32_t);
static void send_packet_out(client_t *, uint32_t, const void *, uint16_t);
static void handle_hello(client_t *);
static void handle_error(client_t *);
static void handle_feature_res(client_t *);
static void handle_multipart_res(client_t *);
static void handle_echo_req(client_t *);
static void handle_packet_in(client_t *);
static void handle_port_status(client_t *);
static void send_poll(client_t *, uint32_t);
static void handle_poll(client_t *, uint32_t, const switch_poll_t *);

void init_openflow(void) {
    if (graph != NULL) {
        fprintf(stderr, "init_openflow() should only be called once\n");
        exit(-1);
    }
    graph = init_graph();
}

void init_connection(client_t *client) {
    ofp_header_t *hello;

    add_vertex_sw(graph, client->fd);
    hello = make_packet(OFPT_HELLO, sizeof(ofp_header_t), 666);
    client_write(client, hello, sizeof(ofp_header_t));
    setup_table_miss(client);
}

void finish_setup(void) {
    setup_done = 1;
    printf("Setup phase finished\n");
    setup_broadcast();
}

void setup_table_miss(client_t *client) {
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

    client_write(client, pack, length);
}

void setup_broadcast(void) {
    port_list_t **mst;
    int firstfd, fd;

    // Find the first fd that's valid
    for (firstfd = 0; (size_t)firstfd <= graph->max_vertex; firstfd++) {
        // I am breaking the abstraction here. Sorry
        if (graph->vertices_sw[firstfd].present) {
            break;
        }
    }
    if ((size_t)firstfd > graph->max_vertex) {
        return;
    }

    mst = init_mst(graph->max_vertex);
    walk_shortest_path(graph, firstfd, IGNORE_LINK, mst, 1, add_mst_edge);

    for (fd = 0; (size_t)fd <= graph->max_vertex; fd++) {
        if (graph->vertices_sw[fd].present) {
            add_broadcast_rule(fd, mst[fd]);
        }
    }

    free_mst(mst, graph->max_vertex);
}

void add_broadcast_rule(int clientfd, const port_list_t *ports) {
    client_t *client;
    ofp_header_t *pack;
    flow_mod_t *flow_mod;
    match_t *match;
    instr_write_t *instr;
    action_output_t *action;
    uint16_t packet_length, instr_length;
    const port_list_t *node;

    client = &of_clients[clientfd];

    instr_length = sizeof(instr_write_t);
    for (node = ports; node != NULL; node = node->next) {
        instr_length += sizeof(instr_write_t);
    }
    instr_length =
        sizeof(instr_write_t) + sizeof(action_output_t);  // TODO: remove
    packet_length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                    sizeof(match_t) + instr_length;

    pack = make_packet(OFPT_FLOW_MOD, packet_length, 0);
    flow_mod = (flow_mod_t *)pack->data;
    match = flow_mod->match;
    instr = (instr_write_t *)((uint8_t *)match + sizeof(match_t));

    flow_mod->table_id = 1;
    flow_mod->command = FM_CMD_ADD;
    flow_mod->buffer_id = htonl(OFP_NO_BUFFER);
    flow_mod->priority = htons(0);
    flow_mod->out_port = OFPP_ANY;
    flow_mod->out_group = OFPP_ANY;

    match->type = htons(OFPMT_OXM);
    match->length = htons(4);

    instr->type = htons(OFPIT_WRITE_ACTIONS);
    instr->length = htons(instr_length);

    /* Add a write action for each port */
    /*
    for (action = instr->actions, node = ports; node != NULL;
         node = node->next, action++) {
        action->type = htons(OFPAT_OUTPUT);
        action->length = htons(sizeof(action_output_t));
        action->port = htonl(node->port);
        action->max_len = htons(0xffff);
    }
    */
    action = instr->actions;
    action->type = htons(OFPAT_OUTPUT);
    action->length = htons(sizeof(action_output_t));
    action->port = htonl(OFPP_ALL);
    action->max_len = htons(0xffff);

    client_write(client, pack, packet_length);
}

void add_unicast_rule(int clientfd, uint32_t port, void *mac) {
    /*
     * I also need to add a table-miss on table 0 that sends us to table 1, and
     *   outputs to controller
     */

    client_t *client;

    client = &of_clients[clientfd];
    add_source_mac_rule(client, mac);
    add_dest_mac_rule(client, mac, port);
}

void add_source_mac_rule(client_t *client, const void *mac) {
    ofp_header_t *pack;
    flow_mod_t *flow_mod;
    match_t *match;
    instr_goto_t *instr;
    uint32_t *fields;
    uint16_t length = sizeof(ofp_header_t) + sizeof(flow_mod_t) +
                      sizeof(match_t) + 8 + sizeof(instr_goto_t);

    pack = make_packet(OFPT_FLOW_MOD, length, 0);
    flow_mod = (flow_mod_t *)pack->data;
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

    client_write(client, pack, length);
}

void add_dest_mac_rule(client_t *client, const void *mac, uint32_t port_id) {
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
    flow_mod->table_id = 1;
    flow_mod->command = FM_CMD_ADD;
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

    client_write(client, pack, length);
}

void handle_ofp_packet(client_t *client) {
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

    if ((hdr = calloc(1, length)) == NULL) {
        perror("calloc");
        exit(-1);
    }

    hdr->version = 0x04;
    hdr->type = type;
    hdr->length = htons(length);
    hdr->xid = xid;

    return hdr;
}

void handle_hello(client_t *client) {
    ofp_header_t *res;

    res = make_packet(OFPT_FEATURE_REQ, sizeof(ofp_header_t), 777);
    client_write(client, res, sizeof(ofp_header_t));
}

void handle_error(client_t *client) {
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

void handle_feature_res(client_t *client) {
    const switch_features_t *features;
    ofp_header_t *mp_pack;
    multipart_t *req;

    if (client->cur_packet->length < sizeof(switch_features_t)) {
        fprintf(stderr, "Feature packet too short\n");
        return;
    }
    features = (switch_features_t *)client->cur_packet->data;

    memcpy(client->uid, features->datapath_id, sizeof(client->uid));

    // Send a multipart port stats request
    mp_pack = make_packet(OFPT_MULTIPART_REQ,
                          sizeof(ofp_header_t) + sizeof(multipart_t), 888);
    req = (multipart_t *)mp_pack->data;
    memset(req, 0, sizeof(multipart_t));
    req->type = htons(OFPMP_PORT_DESC);
    client_write(client, mp_pack, sizeof(ofp_header_t) + sizeof(multipart_t));
}

void handle_multipart_res(client_t *client) {
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
            send_poll(client, ntohl(ports[ndx].port_id));
        }
    }
}

void handle_echo_req(client_t *client) {
    const ofp_header_t *req;
    ofp_header_t *res;
    uint16_t length;

    req = client->cur_packet;
    /* We guarantee that a processed packet will never have
     * length < sizeof(ofp_header_t) */
    length = req->length;

    res = make_packet(OFPT_ECHO_RES, length, client->cur_packet->xid);
    memcpy(res->data, req->data, length - sizeof(ofp_header_t));

    client_write(client, res, length);
}

void handle_packet_in(client_t *client) {
    packet_in_t *pack;
    match_t *match;
    uint32_t *fields;
    uint8_t *data;
    uint32_t port_id;
    uint64_t mac; /* We're going to cram an ethernet address into a long */

    if (client->cur_packet->length <
        sizeof(ofp_header_t) + sizeof(packet_in_t)) {
        fprintf(stderr, "packet_in too short\n");
        return;
    }
    pack = (packet_in_t *)client->cur_packet->data;
    match = &pack->match;
    data =
        (uint8_t *)&pack->match + (ntohs(pack->match.length) + 7) / 8 * 8 + 2;

    fields = (uint32_t *)match->oxm_fields;
    /* We only care about PACKET_IN's that have the in port */
    if (((htonl(fields[0]) >> 9) & 0x7f) != OFPXMT_OFB_IN_PORT) {
        return;
    }

    port_id = ntohl(fields[1]);
    if (ntohl(*(uint32_t *)data) == SWITCH_POLL_MAGIC) {
        handle_poll(client, port_id, (const switch_poll_t *)data);
    } else if (setup_done) {
        /* TODO: we need to be sure this is NOT a switch port */
        /* Put the mac address in the lower 6 bytes of mac */
        mac = ((uint64_t)data[6] << 40) | ((uint64_t)data[7] << 32) |
              ((uint64_t)data[8] << 24) | ((uint64_t)data[9] << 16) |
              ((uint64_t)data[10] << 8) | data[11];
        if (!ts_contains(seen_hosts, mac)) {
            seen_hosts = ts_insert(seen_hosts, mac);
            walk_shortest_path(graph, client->fd, port_id, &data[6], 0,
                               add_unicast_rule);
        }
    }
}

void send_packet_out(client_t *client, uint32_t port, const void *data,
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

    client_write(client, hdr, total_length);
}

void handle_port_status(client_t *client) {
    const port_status_t *pack;
    char port_name[16];

    if (client->cur_packet->length <
        sizeof(ofp_header_t) + sizeof(port_status_t)) {
        fprintf(stderr, "port_status too short\n");
        return;
    }
    pack = (port_status_t *)client->cur_packet->data;

    snprintf(port_name, sizeof(port_name), "%s", pack->port.name);

    switch (pack->reason) {
        case PORT_MOD:
            if (htonl(pack->port.port_id) <= OFPP_MAX) {
                send_poll(client, ntohl(pack->port.port_id));
            }
            break;
    }
}

/*
 * Switch polling protocol
 * =======================
 *
 * On network startup, each switch will send out a poll packet to all connected
 * ports. The poll packets will be sent back to the controller by the switch on
 * the other side of the link.
 *
 * Poll packets are identified by the 4-byte magic value SWITCH_POLL_MAGIC.
 */

void send_poll(client_t *client, uint32_t port) {
    switch_poll_t switch_poll;

    memset(&switch_poll, 0, sizeof(switch_poll));
    switch_poll.magic = htonl(SWITCH_POLL_MAGIC);
    switch_poll.fd = client->fd;
    switch_poll.port_id = port;

    send_packet_out(client, port, &switch_poll, sizeof(switch_poll));
}

void handle_poll(client_t *client, uint32_t port,
                 const switch_poll_t *switch_poll) {
    add_edge_sw(graph, client->fd, port, switch_poll->fd, switch_poll->port_id);
}
