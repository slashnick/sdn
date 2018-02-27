#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#include "client.h"

void init_openflow(void);
void init_connection(client_t *);
void handle_ofp_packet(client_t *);
void finish_setup(void);

#endif /* OPENFLOW_H_ */
