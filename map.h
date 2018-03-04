#ifndef MAP_H_
#define MAP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef union {
    uint64_t u64;
    void *ptr;
} map_val_t;

extern const map_val_t empty_val;

void *map_create(void);
void map_set(void *, uint64_t, map_val_t);
map_val_t map_get(const void *, uint64_t);
void map_remove(void *, uint64_t);
uint8_t map_empty(const void *);
uint64_t map_min(const void *);
uint8_t map_contains(const void *, uint64_t);
void map_free(void *);

#ifdef __cplusplus
}
#endif

#endif /* MAP_H_ */
