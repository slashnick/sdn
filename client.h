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
} client_t;

void init_client(client_t *, int);
void client_write(client_t *, const void *buf, size_t count);
void handle_read_event(client_t *);
void handle_write_event(client_t *);

#endif /* CLIENT_H_ */
