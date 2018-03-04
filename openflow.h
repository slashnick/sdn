#ifndef OPENFLOW_H_
#define OPENFLOW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"

extern client_t *of_clients;

void init_openflow(void);
void init_connection(client_t *);
void handle_ofp_packet(client_t *);
void finish_setup(void);

#ifdef __cplusplus
}
#endif

#endif /* OPENFLOW_H_ */
