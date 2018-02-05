#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CLIENT_STATE_WAITING_HEADER 1
#define CLIENT_STATE_WAITING_PACKET 2

#define READ_PARTIAL 1
#define READ_COMPLETE 2
#define READ_EOF 3
#define READ_STOP 4

static void handle_header(client_t *);
static void handle_packet(client_t *);
static int read_into_buffer(client_t *);

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
    int status;

    for (;;) {
        status = read_into_buffer(client);
        if (status == READ_STOP) {
            break;
        } else if (status == READ_EOF) {
            /* TODO */
            printf("Got EOF\n");
            break;
        } else if (status == READ_COMPLETE) {
            switch (client->state) {
                case CLIENT_STATE_WAITING_HEADER:
                    handle_header(client);
                    break;
                case CLIENT_STATE_WAITING_PACKET:
                    handle_packet(client);
                    break;
                default:
                    fprintf(stderr, "Unexpected client state: %d\n",
                            client->state);
                    exit(-1);
            }
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
    }
    /* Do not increment pos, we want to start reading just past the header */
}

void handle_packet(client_t *client) {
    printf("got a packet\n");
    switch (client->cur_packet->type) {}

    client->state = CLIENT_STATE_WAITING_HEADER;
    free(client->cur_packet);
    client->bufsize = sizeof(ofp_header_t);
    client->pos = 0;
    if ((client->cur_packet = malloc(client->bufsize)) == NULL) {
        perror("malloc");
        exit(-1);
    }
}

/* Read from the client into the client's buffer.
 *
 * Return READ_COMPLETE if the buffer is now full
 * Return READ_PARTIAL if read was successful, but the buffer is not full
 * Return READ_STOP if we encountered an EAGAIN
 */
int read_into_buffer(client_t *client) {
    ssize_t status;

    if (client->pos == client->bufsize) {
        /* If there is nothing to read, do nothing */
        return READ_COMPLETE;
    }

    status = read(client->fd, (uint8_t *)client->cur_packet + client->pos,
                  client->bufsize - client->pos);
    if (status < 0) {
        /* We should never continue past this if statement */
        if (errno == EAGAIN) {
            /* Do nothing */
            return READ_STOP;
        } else {
            perror("read");
            exit(-1);
        }
    } else if (status == 0) {
        /* EOF */
        return READ_EOF;
    }

    client->pos += status;

    return (client->pos == client->bufsize) ? READ_COMPLETE : READ_PARTIAL;
}
