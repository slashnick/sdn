#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#include <stdint.h>
#include <map>
#include "client.h"

extern std::map<uint64_t, Client *> client_table;

void init_connection(Client *);
void handle_ofp_packet(Client *);
void send_packet_out(Client *, uint32_t, const void *, uint16_t);
void finish_setup();

#endif /* OPENFLOW_H_ */
