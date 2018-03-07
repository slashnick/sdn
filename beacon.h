#ifndef BEACON_H_
#define BEACON_H_

#include <stdint.h>
#include "client.h"

const uint8_t SWITCH_POLL_MAGIC[7] = "\x50\x05\xa1\xc0\xff\xee";

typedef struct {
    uint8_t magic[6];
    uint32_t poll_id;
    uint32_t uid1;
    uint32_t uid2;
    uint32_t port_id;
} __attribute__((packed)) switch_poll_t;

void send_polls(void*);
void recv_poll(Client*, uint32_t, const uint8_t* data);

#endif /* BEACON_H_ */
