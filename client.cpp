#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "event.h"
#include "openflow.h"

#define CLIENT_STATE_WAITING_HEADER 1
#define CLIENT_STATE_WAITING_PACKET 2

#define READ_PARTIAL 1
#define READ_COMPLETE 2
#define READ_EOF 3
#define READ_STOP 4

Write::Write(uint8_t *d, uint16_t s) {
    data = d;
    pos = 0;
    size = s;
}

Client::Client(int f, void *s) {
    fd = f;
    server = s;
    canwrite = 0;
    state = CLIENT_STATE_WAITING_HEADER;
    bufsize = sizeof(ofp_header_t);
    pos = 0;
    cur_packet = (ofp_header_t *)malloc(bufsize);
    if (cur_packet == nullptr) {
        perror("malloc");
        exit(-1);
    }
}

void Client::init() {
    init_connection(this);
}

/* Note: client will now own buf, so don't use buf after making this call */
void Client::write_packet(void *buf, uint16_t count) {
    write_queue.push(new Write(reinterpret_cast<uint8_t *>(buf), count));
}

/* Read some data into the buffer, and handle  */
void Client::handle_read_event() {
    int status;

    for (;;) {
        status = read_into_buffer();
        if (status == READ_STOP) {
            break;
        } else if (status == READ_EOF) {
            close_client();
            break;
        } else if (status == READ_COMPLETE) {
            switch (state) {
                case CLIENT_STATE_WAITING_HEADER:
                    handle_header();
                    break;
                case CLIENT_STATE_WAITING_PACKET:
                    handle_packet();
                    break;
                default:
                    fprintf(stderr, "Unexpected client state: %d\n", state);
                    exit(-1);
            }
        }
    }
}

void Client::flush_write_queue() {
    ssize_t status;

    while (canwrite && !write_queue.empty()) {
        Write w = write_queue.front();
        status = write(fd, w.data + w.pos, w.size - w.pos);
        if (status < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                canwrite = 0;
            } else {
                fprintf(stderr, "write(%d): %s\n", fd, strerror(errno));
                close_client();
                break;
            }
        } else {
            w.pos += status;
            if (w.pos == w.size) {
                // Complete write. Dequeue
                free(w.data);
                write_queue.pop();
            }
        }
    }
}

void Client::handle_header() {
    cur_packet->length = ntohs(cur_packet->length);

    if (cur_packet->length > sizeof(ofp_header_t)) {
        bufsize = cur_packet->length;
        cur_packet = (ofp_header_t *)realloc(cur_packet, bufsize);
        if (cur_packet == nullptr) {
            perror("realloc");
            exit(-1);
        }
    } else if (cur_packet->length < sizeof(ofp_header_t)) {
        cur_packet->length = sizeof(ofp_header_t);
    }
    state = CLIENT_STATE_WAITING_PACKET;
    /* Do not increment pos, we want to start reading just past the header */
}

void Client::handle_packet() {
    // printf("got an OFP packet: type=%d\n", this->cur_packet->type);
    handle_ofp_packet(this);

    state = CLIENT_STATE_WAITING_HEADER;
    free(cur_packet);
    bufsize = sizeof(ofp_header_t);
    pos = 0;
    cur_packet = (ofp_header_t *)(malloc(bufsize));
    if (cur_packet == nullptr) {
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
int Client::read_into_buffer() {
    ssize_t status;

    if (pos == bufsize) {
        /* If there is nothing to read, do nothing */
        return READ_COMPLETE;
    }

    status = read(fd, (uint8_t *)cur_packet + pos, bufsize - pos);
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

    pos += status;

    return (pos == bufsize) ? READ_COMPLETE : READ_PARTIAL;
}

void Client::close_client() {
    free(cur_packet);
    cur_packet = nullptr;

    /* Free any queued writes */
    while (!write_queue.empty()) {
        free(write_queue.front().data);
        write_queue.pop();
    }

    if (close(fd) < 0) {
        perror("close");
    }

    Server *s = (Server *)server;
    s->clients.erase(fd);
}
