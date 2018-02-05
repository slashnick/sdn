#include "event.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include "client.h"

static void nonblock(int);

void init_server(server_t *server, uint16_t port) {
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

    /* Set up server */
    server->fd = sock;
    server->maxfd = 127;
    server->clients = malloc((server->maxfd + 1) * sizeof(client_t));
    if (server->clients == NULL) {
        perror("malloc");
        exit(-1);
    }
}

void listen_and_serve(server_t *server) {
#define MAX_EVENTS 10
    struct epoll_event ev, events[MAX_EVENTS];
    int clientfd, ep, nfds;
    size_t ndx;
    client_t *client;

    if ((ep = epoll_create1(0)) < 0) {
        perror("epoll_create1");
        exit(-1);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = server->fd;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, server->fd, &ev) < 0) {
        perror("epoll_ctl: listen");
        exit(-1);
    }

    for (;;) {
        if ((nfds = epoll_wait(ep, events, MAX_EVENTS, -1)) < 0) {
            perror("epoll_wait");
            exit(-1);
        }
        for (ndx = 0; ndx < (size_t)nfds; ndx++) {
            if (events[ndx].data.fd == server->fd) {
                if ((clientfd = accept(server->fd, NULL, NULL)) < 0) {
                    perror("accept");
                    exit(-1);
                }
                nonblock(clientfd);
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = clientfd;

                if (epoll_ctl(ep, EPOLL_CTL_ADD, clientfd, &ev) < 0) {
                    perror("epoll_ctl: accept");
                    exit(-1);
                }

                if ((size_t)clientfd > server->maxfd) {
                    /* Dynamically grow client list as we get more clients */
                    while ((size_t)clientfd > server->maxfd) {
                        server->maxfd = server->maxfd * 2 + 1;
                    }
                    server->clients =
                        realloc(server->clients,
                                (server->maxfd + 1) * sizeof(client_t));
                    if (server->clients == NULL) {
                        perror("realloc");
                        exit(-1);
                    }
                }

                client = &server->clients[clientfd];
                init_client(client, clientfd);
            } else {
                assert((size_t)events[ndx].data.fd < server->maxfd);
                client = &server->clients[events[ndx].data.fd];
                if (events[ndx].events & EPOLLOUT) {
                    client->canwrite = 1;
                }
                if (events[ndx].events & EPOLLIN) {
                    handle_read_event(client);
                }
            }
        }
    }
}

void close_server(server_t *server) {
    if (close(server->fd) < 0) {
        perror("close");
        exit(-1);
    }
    free(server->clients);
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
