#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>
#include <queue>
#include <set>

typedef struct {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t xid;
    uint8_t data[];
} __attribute__((packed)) ofp_header_t;

class Write {
   public:
    uint8_t* data;
    uint16_t pos;
    uint16_t size;
    Write(uint8_t*, uint16_t);
};

/*
 * Clients are state machines. A client is either waiting to receive a header,
 * or it is waiting to receive a packet.
 * It is always known how many bytes a client has to receive for a given state.
 */
class Client {
   public:
    Client(int, void*);
    void init();
    void write_packet(void*, uint16_t);
    void handle_read_event();
    void flush_write_queue();
    uint64_t uid;
    ofp_header_t* cur_packet;
    void* server;
    uint8_t canwrite;
    std::set<uint32_t> ports;
    std::set<uint32_t> sw_ports;
    uint8_t has_mst;

   private:
    void handle_header();
    void handle_packet();
    int read_into_buffer();
    void close_client();
    int fd;
    uint16_t bufsize;
    uint16_t pos;
    std::queue<Write> write_queue;
    uint8_t state;
};

#endif /* CLIENT_H_ */
