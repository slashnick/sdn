#include "map.h"
#include <map>

typedef std::map<uint64_t, map_val_t> Map;

extern "C" {

const map_val_t empty_val = {0};

void* map_create(void) {
    return reinterpret_cast<void*>(new Map);
}

void map_set(void* map, uint64_t key, map_val_t val) {
    Map* m = reinterpret_cast<Map*>(map);
    m->insert(std::pair<uint64_t, map_val_t>(key, val));
}

map_val_t map_get(const void* map, uint64_t key) {
    const Map* m = reinterpret_cast<const Map*>(map);
    Map::const_iterator search = m->find(key);
    if (search != m->end()) {
        map_val_t val;
        val.u64 = search->first;
        return val;
    } else {
        return empty_val;
    }
}

void map_remove(void* map, uint64_t key) {
    Map* m = reinterpret_cast<Map*>(map);
    m->erase(key);
}

uint8_t map_empty(const void* map) {
    const Map* m = reinterpret_cast<const Map*>(map);
    return m->empty();
}

uint64_t map_min(const void* map) {
    const Map* m = reinterpret_cast<const Map*>(map);
    return m->begin()->first;
}

uint8_t map_contains(const void* map, uint64_t key) {
    const Map* m = reinterpret_cast<const Map*>(map);
    return m->find(key) != m->end();
}

void map_free(void* map) {
    Map* m = reinterpret_cast<Map*>(map);
    delete m;
}
}
