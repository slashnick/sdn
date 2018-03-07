#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#include <stdint.h>
#include <map>
#include "client.h"

extern std::set<uint64_t> seen_hosts;
extern std::map<uint64_t, Client *> client_table;

enum ofp_group_mod_command { OFPGC_ADD = 0, OFPGC_MODIFY = 1 };

void init_connection(Client *);
void handle_ofp_packet(Client *);
void add_broadcast_rule(Client *);
void send_packet_out(Client *, uint32_t, const void *, uint16_t);
void update_broadcast_group(Client *, const std::set<uint32_t> *, uint16_t);
void add_dest_mac_rule(Client *, const void *mac, uint32_t port_id, uint8_t cmd,
                       uint8_t table_id);

#endif /* OPENFLOW_H_ */
