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
};

typedef struct {
    uint8_t datapath_id[8];
    uint32_t n_buffers;
    uint8_t n_tables;
    uint8_t auxiliary_id;
    uint8_t _pad[3];
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

static ofp_header_t *make_packet(uint8_t, uint16_t, uint32_t);
static void handle_hello(client_t *);
static void handle_feature_res(client_t *);
static void handle_echo_req(client_t *);
static void handle_packet_in(client_t *);

void handle_ofp_packet(client_t *client) {
    switch (client->cur_packet->type) {
        case OFPT_HELLO:
            handle_hello(client);
            break;
        case OFPT_FEATURE_RES:
            handle_feature_res(client);
            break;
        case OFPT_ECHO_REQ:
            handle_echo_req(client);
            break;
        case OFPT_PACKET_IN:
            handle_packet_in(client);
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
    const ofp_header_t *req;
    ofp_header_t *res;

    req = client->cur_packet;

    res = make_packet(OFPT_FEATURE_REQ, sizeof(ofp_header_t), req->xid);
    client_write(client, res, sizeof(ofp_header_t));
}

void handle_feature_res(client_t *client) {
    const switch_features_t *features;

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
    const packet_in_t *pack;

    if (client->cur_packet->length < sizeof(packet_in_t)) {
        fprintf(stderr, "packet_in too short\n");
        return;
    }
    pack = (packet_in_t *)client->cur_packet->data;

    printf("Got packet in\n");
}
