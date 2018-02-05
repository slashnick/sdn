#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>

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
    uint8_t *buffer;
} client_t;

void init_client(client_t *, int);
void handle_read_event(client_t *);
void handle_write_event(client_t *);

#endif /* CLIENT_H_ */
