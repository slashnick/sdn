#include "event.h"
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdio>
#include <map>
#include <queue>
#include <tuple>
#include <vector>
#include "client.h"
#include "openflow.h"

static uint64_t current_time_ms(void);
static void nonblock(int);

void Server::open(uint16_t port) {
    int sock, one = 1;
    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    signal(SIGPIPE, SIG_IGN);
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
    if (bind(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) <
        0) {
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
    fd = sock;
}

/* Return the number of milliseconds since EPOCH */
uint64_t current_time_ms(void) {
    struct timeval tv;

    gettimeofday(&tv, nullptr);
    return static_cast<uint64_t>(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

void Server::listen_and_serve() {
#define MAX_EVENTS 10
    struct epoll_event ev, events[MAX_EVENTS];
    int clientfd, ep, nfds, timeout;

    if ((ep = epoll_create1(0)) < 0) {
        perror("epoll_create1");
        exit(-1);
    }

    /* Silence valgrind */
    memset(&ev.data, 0, sizeof(ev.data));

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) < 0) {
        perror("epoll_ctl: listen");
        exit(-1);
    }

    for (;;) {
        // Time until next time-based event fires
        timeout = -1;
        if (!time_events.empty()) {
            uint64_t now = current_time_ms();
            Event next_event = time_events.top();
            if (now < next_event.when) {
                timeout = static_cast<int>(next_event.when - now);
            } else {
                timeout = 0;
            }
        }

        if ((nfds = epoll_wait(ep, events, MAX_EVENTS, timeout)) < 0) {
            perror("epoll_wait");
            exit(-1);
        }
        handle_time_events();
        int ndx;
        for (ndx = 0; ndx < nfds; ndx++) {
            if (events[ndx].data.fd == fd) {
                if ((clientfd = accept(fd, nullptr, nullptr)) < 0) {
                    perror("accept");
                }
                nonblock(clientfd);
                ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
                ev.data.fd = clientfd;

                if (epoll_ctl(ep, EPOLL_CTL_ADD, clientfd, &ev) < 0) {
                    perror("epoll_ctl: accept");
                    exit(-1);
                }

                Client* c = new Client(clientfd, this);
                clients.insert(std::pair<int, Client*>(clientfd, c));
                c->init();
            } else {
                Client* c = clients.find(events[ndx].data.fd)->second;
                if (events[ndx].events & EPOLLIN) {
                    c->handle_read_event();
                }
                if (events[ndx].events & EPOLLOUT) {
                    c->canwrite = 1;
                }
                c->flush_write_queue();
            }
        }
    }
}

Event::Event(event_handler_t h, void* a, uint64_t w) {
    handler = h;
    arg = a;
    when = w;
}

void Server::schedule_event(uint64_t when_ms, event_handler_t handler,
                            void* arg) {
    time_events.push(Event(handler, arg, current_time_ms() + when_ms));
}

void Server::handle_time_events() {
    uint64_t now = current_time_ms();
    while (!time_events.empty()) {
        Event event = time_events.top();
        if (event.when > now) {
            break;
        }
        time_events.pop();

        event_handler_t handler = event.handler;
        void* arg = event.arg;

        handler(arg);
    }
}

void Server::close_server() {
    if (close(fd) < 0) {
        perror("close");
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
