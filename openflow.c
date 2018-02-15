#include "openflow.h"
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum ofp_type {
    OFPT_HELLO = 0,
    OFPT_ERROR = 1,
    OFPT_ECHO_REQ = 2,
    OFPT_ECHO_RES = 3,
    OFPT_FEATURE_REQ = 5,
    OFPT_FEATURE_RES = 6,
    OFPT_PACKET_IN = 10,
    OFPT_PORT_STATUS = 12,
    OFPT_MULTIPART_REQ = 18,
    OFPT_MULTIPART_RES = 19,
};

enum port_reason {
    PORT_ADD = 0,
    PORT_DEL = 1,
    PORT_MOD = 2,
};

enum multipart_type {
    OFPMP_PORT_DESC = 13,
};

typedef struct {
    uint16_t type;
    uint16_t code;
} ofp_error_t;

typedef struct {
    uint8_t datapath_id[8];
    uint32_t n_buffers;
    uint8_t n_tables;
    uint8_t auxiliary_id;
    uint8_t _pad[2];
    uint32_t capabilities;
    uint32_t actions;
} switch_features_t;

typedef struct {
    uint32_t buffer_id;
    uint16_t total_len;
    uint16_t in_port;
    uint8_t reason;
    uint8_t _pad;
    uint8_t data[6];
} packet_in_t;

typedef struct {
    uint16_t type;
    uint16_t flags;
    uint8_t _pad[4];
    uint8_t body[];
} multipart_t;

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
} port_t;

typedef struct {
    uint8_t reason;
    uint8_t _pad[7];
    port_t port;
} port_status_t;

static ofp_header_t *make_packet(uint8_t, uint16_t, uint32_t);
static void handle_hello(client_t *);
static void handle_error(client_t *);
static void handle_feature_res(client_t *);
static void handle_multipart_res(client_t *);
static void handle_echo_req(client_t *);
static void handle_packet_in(client_t *);
static void handle_port_status(client_t *);

void init_connection(client_t *client) {
    ofp_header_t *hello;

    hello = make_packet(OFPT_HELLO, sizeof(ofp_header_t), 666);
    client_write(client, hello, sizeof(ofp_header_t));
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

    if ((hdr = malloc(length)) == NULL) {
        perror("malloc");
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
    ofp_error_t *err;

    err = (ofp_error_t *)client->cur_packet->data;

    printf("Error: type=0x%04x code=0x%04x\n", ntohs(err->type), ntohs(err->code));
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

    printf("New switch\n");
    printf("  Openflow version: ");
    switch (client->cur_packet->version) {
        case 0x01:
            printf("1.0");
            break;
        case 0x02:
            printf("1.1");
            break;
        case 0x03:
            printf("1.2");
            break;
        case 0x04:
            printf("1.3");
            break;
        case 0x05:
            printf("1.4");
            break;
        default:
            printf("Unknown (0x%02d)", client->cur_packet->version);
            break;
    }
    printf("\n");
    printf("  datapath-id: 0x%02x%02x%02x%02x%02x%02x%02x%02x\n",
           client->uid[0], client->uid[1], client->uid[2], client->uid[3],
           client->uid[4], client->uid[5], client->uid[6], client->uid[7]);

    // Send a multipart port stats request
    mp_pack = make_packet(OFPT_MULTIPART_REQ, sizeof(ofp_header_t) + sizeof(multipart_t), 888);
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

    mp = (multipart_t *)client->cur_packet->data;
    if (ntohs(mp->type) != OFPMP_PORT_DESC) {
        return;
    }

    ports = (port_t *)mp->body;
    num_ports = (client->cur_packet->length - sizeof(ofp_header_t) - sizeof(multipart_t)) / sizeof(port_t);
    printf("  %ld ports:\n", num_ports);
    for (ndx = 0; ndx < num_ports; ndx++) {
        snprintf(port_name, sizeof(port_name), "%s", ports[ndx].name);
        printf("    %s (0x%04x)\n", port_name, htonl(ports[ndx].port_id));
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

void handle_packet_in(client_t *_client) {
    printf("Got packet in\n");
}

void handle_port_status(client_t *client) {
    const port_status_t *pack;
    char port_name[16];

    if (client->cur_packet->length < sizeof(port_status_t)) {
        fprintf(stderr, "port_status too short\n");
        return;
    }
    pack = (port_status_t *)client->cur_packet->data;

    snprintf(port_name, sizeof(port_name), "%s", pack->port.name);

    switch (pack->reason) {
        case PORT_ADD:
            printf("Add ");
            break;
        case PORT_DEL:
            printf("Delete ");
            break;
        case PORT_MOD:
            printf("Modify ");
            break;
        default:
            printf("(Unknown) ");
            break;
    }
    printf("port %s (0x%04x)\n", port_name, ntohl(pack->port.port_id));
}
