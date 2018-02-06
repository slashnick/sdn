#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t xid;
    uint8_t data[];
} __attribute__((packed)) ofp_header_t;

typedef struct queued_write {
    struct queued_write *next;
    uint8_t *data;
    uint16_t size;
} queued_write_t;

/*
 * Clients are state machines. A client is either waiting to receive a header,
 * or it is waiting to receive a packet.
 * It is always known how many bytes a client has to receive for this state.
 */
typedef struct {
    int fd;
    uint8_t canwrite;
    uint8_t state;
    uint16_t bufsize;
    uint16_t pos;
    ofp_header_t *cur_packet;
    queued_write_t *write_queue_head;
    queued_write_t *write_queue_tail;
} client_t;

void init_client(client_t *, int);
void client_write(client_t *, void *, uint16_t);
void handle_read_event(client_t *);
void flush_write_queue(client_t *);

#endif /* CLIENT_H_ */
