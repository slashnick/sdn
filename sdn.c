#include <stdio.h>
#include <stdlib.h>
#include "event.h"

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
    listen_and_serve(&server);
    close_server(&server);

    return 0;
}
