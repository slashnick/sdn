#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include "event.h"

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
    server_t server;
    long port;

    if (argc < 2) {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return 1;
    }

#define MAX_PORT 65535
    port = strtol(argv[1], NULL, 10);
    if (port < 0 || port > MAX_PORT) {
        fprintf(stderr, "%s: invalid port number\n", argv[1]);
        return 1;
    }

    if (setvbuf(stdout, NULL, _IONBF, 0) != 0) {
        perror("setvbuf");
        return 0;
    }

    init_server(&server, (uint16_t)port);
    printf("Listening on port %ld\n", port ? port : socket_port(server.fd));
    listen_and_serve(&server);
    close_server(&server);

    return 0;
}
