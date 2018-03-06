#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#include "client.h"

void init_connection(Client *);
void handle_ofp_packet(Client *);
void finish_setup();

#endif /* OPENFLOW_H_ */
