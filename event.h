#ifndef EVENT_H_
#define EVENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdlib.h>
#include "client.h"

typedef struct {
    size_t maxfd;
    client_t *clients;
    void *time_events;
    int fd;
} server_t;

typedef void (*event_handler_t)(void *arg);

void init_server(server_t *, uint16_t);
void listen_and_serve(server_t *);
void schedule_event(server_t *, uint64_t, event_handler_t, void *);
void close_server(server_t *);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_H_ */
