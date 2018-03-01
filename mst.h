#ifndef MST_H_
#define MST_H_

#include <stddef.h>
#include <stdint.h>

#define IGNORE_LINK 0xffffffff

typedef struct port_list {
    struct port_list *next;
    uint32_t port;
} port_list_t;

port_list_t **init_mst(size_t);
void add_mst_edge(int, uint32_t, void *);
void free_mst(port_list_t **, size_t);

#endif /* MST_H_ */
