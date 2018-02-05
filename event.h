#ifndef EVENT_H_
#define EVENT_H_

#include <stdint.h>
#include <stdlib.h>
#include "client.h"

typedef struct {
    size_t maxfd;
    client_t *clients;
    int fd;
} server_t;

void init_server(server_t *, uint16_t);
void listen_and_serve(server_t *);
void close_server(server_t *);

#endif /* EVENT_H_ */
