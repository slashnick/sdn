#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "openflow.h"

#define CLIENT_STATE_WAITING_HEADER 1
#define CLIENT_STATE_WAITING_PACKET 2

#define READ_PARTIAL 1
#define READ_COMPLETE 2
#define READ_EOF 3
#define READ_STOP 4

static void handle_header(client_t *);
static void handle_packet(client_t *);
static int read_into_buffer(client_t *);
static void close_client(client_t *);
static void enqueue_write(client_t *, void *, uint16_t);
static void dequeue_write(client_t *);

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
    client->write_queue_head = NULL;
    client->write_queue_tail = NULL;
    client->sw_ports = NULL;
    client->ports = NULL;

    init_connection(client);
}

/* Note: client will now own buf, so don't use buf after making this call */
void client_write(client_t *client, void *buf, uint16_t count) {
    enqueue_write(client, buf, count);
}

/* Read some data into the buffer, and handle  */
void handle_read_event(client_t *client) {
    int status;

    for (;;) {
        status = read_into_buffer(client);
        if (status == READ_STOP) {
            break;
        } else if (status == READ_EOF) {
            close_client(client);
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

void flush_write_queue(client_t *client) {
    queued_write_t *node;
    ssize_t status;

    while (client->canwrite && client->write_queue_head != NULL) {
        node = client->write_queue_head;
        status = write(client->fd, (uint8_t *)node->data + node->pos,
                       node->size - node->pos);
        if (status < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                client->canwrite = 0;
            } else {
                fprintf(stderr, "write(%d): %s", client->fd, strerror(errno));
                close_client(client);
                break;
            }
        } else {
            node->pos += status;
            if (node->pos == node->size) {
                /* Complete write. Clean up node */
                dequeue_write(client);
            }
        }
    }
}

void handle_header(client_t *client) {
    client->cur_packet->length = ntohs(client->cur_packet->length);

    if (client->cur_packet->length > sizeof(ofp_header_t)) {
        client->bufsize = client->cur_packet->length;
        client->cur_packet = realloc(client->cur_packet, client->bufsize);
        if (client->cur_packet == NULL) {
            perror("realloc");
            exit(-1);
        }
    } else if (client->cur_packet->length < sizeof(ofp_header_t)) {
        client->cur_packet->length = sizeof(ofp_header_t);
    }
    client->state = CLIENT_STATE_WAITING_PACKET;
    /* Do not increment pos, we want to start reading just past the header */
}

void handle_packet(client_t *client) {
    handle_ofp_packet(client);

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
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Do nothing */
            return READ_STOP;
        } else {
            perror("read");
            return READ_STOP;
        }
    } else if (status == 0) {
        /* EOF */
        return READ_EOF;
    }

    client->pos += status;

    return (client->pos == client->bufsize) ? READ_COMPLETE : READ_PARTIAL;
}

void close_client(client_t *client) {
    queued_write_t *node;

    free(client->cur_packet);
    client->cur_packet = NULL;

    /* Free any queued writes */
    while (client->write_queue_head != NULL) {
        node = client->write_queue_head;
        client->write_queue_head = client->write_queue_head->next;
        free(node->data);
        free(node);
    }
    client->write_queue_tail = NULL;

    if (close(client->fd) < 0) {
        perror("close");
    }
}

void enqueue_write(client_t *client, void *buf, uint16_t count) {
    queued_write_t *node;

    if ((node = malloc(sizeof(queued_write_t))) == NULL) {
        perror("malloc");
        exit(-1);
    }
    node->next = NULL;
    node->data = buf;
    node->pos = 0;
    node->size = count;

    if (client->write_queue_tail != NULL) {
        client->write_queue_tail->next = node;
        client->write_queue_tail = node;
    } else {
        client->write_queue_head = node;
        client->write_queue_tail = node;
    }
}

void dequeue_write(client_t *client) {
    queued_write_t *node;

    node = client->write_queue_head;
    client->write_queue_head = client->write_queue_head->next;
    if (client->write_queue_tail == node) {
        client->write_queue_tail = NULL;
    }

    /* Node owns its data, so we free it here */
    free(node->data);
    free(node);
}
