#ifndef EVENT_H_
#define EVENT_H_

#include <stdint.h>
#include <stdlib.h>
#include <map>
#include "client.h"
#include "graph.h"

typedef void (*event_handler_t)(void* arg);

// Time-based event
class Event {
   public:
    event_handler_t handler;
    void* arg;
    uint64_t when;
    Event(event_handler_t, void*, uint64_t);
};

class CompareEvents {
   public:
    bool operator()(const Event a, const Event b) {
        return a.when > b.when;
    }
};

class Server {
   public:
    void open(uint16_t);
    void listen_and_serve(void);
    void schedule_event(uint64_t, event_handler_t, void*);
    void close_server();
    Graph graph;  // network graph
    int fd;
    std::map<int, Client*> clients;

   private:
    std::priority_queue<Event, std::vector<Event>, CompareEvents> time_events;
    void handle_time_events(void);
};

#endif /* EVENT_H_ */
