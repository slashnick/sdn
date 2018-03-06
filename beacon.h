#ifndef BEACON_
#define BEACON_

#include <stdint.h>
#include "client.h"

const uint8_t SWITCH_POLL_MAGIC[7] = "\x50\x05\xa1\xc0\xff\xee";

typedef struct {
    uint8_t magic[6];
    uint64_t uid;
    uint32_t port_id;
} __attribute__((packed)) switch_poll_t;

void send_poll(Client*, uint32_t);
void recv_poll(Client*, uint32_t, const uint8_t* data);

#endif /* BEACON_ */
