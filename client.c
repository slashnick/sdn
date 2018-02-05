#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CLIENT_STATE_WAITING_HEADER 1
#define CLIENT_STATE_WAITING_PACKET 2

static void handle_header(client_t *);
static void handle_packet(client_t *);
static void read_into_buffer(client_t *);

void init_client(client_t *client, int fd) {
    client->fd = fd;
    client->canwrite = 0;
    client->state = CLIENT_STATE_WAITING_HEADER;
    client->bufsize = sizeof(ofp_header_t);
    client->pos = 0;
    if ((client->cur_packet = malloc(client->bufsize)) == NULL) {
        perror("malloc");
        exit(-1);
    }
}

/* Read some data into the buffer, and handle  */
void handle_read_event(client_t *client) {
    read_into_buffer(client);

    if (client->pos == client->bufsize) {
        switch (client->state) {
            case CLIENT_STATE_WAITING_HEADER:
                handle_header(client);
                break;
            case CLIENT_STATE_WAITING_PACKET:
                handle_packet(client);
                break;
            default:
                fprintf(stderr, "Unexpected client state: %d\n", client->state);
                exit(-1);
        }
    }
}

void handle_write_event(client_t *client) {
    ((void)client);
}

void handle_header(client_t *client) {
    client->cur_packet->length = ntohs(client->cur_packet->length);

    client->state = CLIENT_STATE_WAITING_PACKET;
    if (client->cur_packet->length > client->bufsize) {
        client->bufsize = client->cur_packet->length;
        client->cur_packet = realloc(client->cur_packet, client->bufsize);
        if (client->cur_packet == NULL) {
            perror("realloc");
            exit(-1);
        }
    } else {
        handle_packet(client);
    }
    /* Do not increment pos, we want to start reading just past the header */
}

void handle_packet(client_t *client) {
    printf("got packet\n");

    client->state = CLIENT_STATE_WAITING_HEADER;
    free(client->cur_packet);
    client->bufsize = sizeof(ofp_header_t);
    client->pos = 0;
    if ((client->cur_packet = malloc(client->bufsize)) == NULL) {
        perror("malloc");
        exit(-1);
    }
}

void read_into_buffer(client_t *client) {
    ssize_t status;

    if (client->pos == client->bufsize) {
        /* If there is nothing to read, do nothing */
        return;
    }

    status = read(client->fd, (uint8_t *)client->cur_packet + client->pos,
                  client->bufsize - client->pos);
    if (status < 0) {
        /* We should never continue past this if statement */
        if (status == EWOULDBLOCK) {
            /* Do nothing */
            return;
        } else {
            perror("read");
            exit(-1);
        }
    }

    client->pos += status;
}
