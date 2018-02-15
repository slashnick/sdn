#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#include "client.h"

void init_connection(client_t *);
void handle_ofp_packet(client_t *);

#endif /* OPENFLOW_H_ */
