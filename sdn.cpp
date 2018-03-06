#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "event.h"
#include "openflow.h"

static uint16_t socket_port(int);
int main(int argc, const char **);

uint16_t socket_port(int sock) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getsockname(sock, (struct sockaddr *)&addr, &addr_len)) {
        perror("getsockname");
        exit(-1);
    }
    if (addr_len != sizeof(addr)) {
        exit(-1);
    }

    return ntohs(addr.sin_port);
}

int main(int argc, const char *argv[]) {
    Server server;
    long port;

    if (argc < 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return 1;
    }

#define MAX_PORT 65535
    port = strtol(argv[1], nullptr, 10);
    if (port < 0 || port > MAX_PORT) {
        fprintf(stderr, "%s: invalid port number\n", argv[1]);
        return 1;
    }

    if (setvbuf(stdout, nullptr, _IONBF, 0) != 0) {
        perror("setvbuf");
        return 0;
    }

    server.open((uint16_t)port);
    printf("Listening on port %ld\n", port ? port : socket_port(server.fd));
    server.listen_and_serve();
    server.close_server();

    return 0;
}
