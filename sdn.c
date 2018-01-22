#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

static int server(uint16_t);
static int listen_and_serve(int);
static void nonblock(int);

int server(uint16_t port) {
    int sock, one = 1;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(-1);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0) {
        perror("setsockopt");
        close(sock);
        exit(-1);
    }
    nonblock(sock);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        exit(-1);
    }
    if (listen(sock, 1)) {
        perror("listen");
        close(sock);
        exit(-1);
    }

    return sock;
}

int listen_and_serve(int sock) {
#define MAX_EVENTS 10
    struct epoll_event ev, events[MAX_EVENTS];
    int client, ep, nfds, ndx;

    if ((ep = epoll_create1(0)) < 0) {
        perror("epoll_create1");
        exit(-1);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = sock;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, sock, &ev) < 0) {
        perror("epoll_ctl: listen");
        exit(-1);
    }

    for (;;) {
        if ((nfds = epoll_wait(ep, events, MAX_EVENTS, -1)) < 0) {
            perror("epoll_wait");
            exit(-1);
        }
        for (ndx = 0; ndx < nfds; ndx++) {
            if (events[ndx].data.fd == sock) {
                if ((client = accept(sock, NULL, NULL)) < 0) {
                    perror("accept");
                    exit(-1);
                }
                nonblock(client);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client;

                if (epoll_ctl(ep, EPOLL_CTL_ADD, client, &ev) < 0) {
                    perror("epoll_ctl: accept");
                    exit(-1);
                }

                printf("accepted new connection: %d\n", client);
            } else {
                printf("Client %d has data\n", events[ndx].data.fd);
            }
        }
    }
}

void nonblock(int fd) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
        perror("fcntl(F_GETFL)");
        return;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK)) {
        perror("fcntl(O_NONBLOCK)");
    }
}

int main(int argc, const char *argv[]) {
    int sock;
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

    sock = server((uint16_t)port);
    listen_and_serve(sock);

    if (close(sock) < 0) {
        perror("close");
        exit(-1);
    }

    return 0;
}
