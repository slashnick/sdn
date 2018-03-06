/* Switch-to-switch link discovery */

#include "beacon.h"
#include <cstdio>
#include "openflow.h"

void send_poll(Client* client, uint32_t port) {
    ((void)client);
    ((void)port);
    printf("sending poll\n");
}

void recv_poll(Client* client, uint32_t port, const uint8_t* data) {
    ((void)client);
    ((void)port);
    ((void)data);
    printf("received poll\n");
}
