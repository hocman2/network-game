#include <utility>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <print>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "net.h"
#include "game.h"

const uint16_t PORT = 12345;
const uint16_t MAX_CLIENTS = 255;

using namespace std;

struct Client {
    int fd = -1;
};

struct Host {
    int fd = -1;
};

static bool net_task_running = true;

static uint16_t num_clients = 0;
static Client clients[MAX_CLIENTS];
static Host host = {};

void stop_net() {
    net_task_running = false;
}

int start_host() {
 
    int host_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (host_fd < 0) {
        println("Failed to create socket");
        return -1;
    }

    host = {
        .fd = host_fd,
    };

    int opt_val = 1;
    setsockopt(host_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

    struct sockaddr_in host;
    host.sin_family = AF_INET;
    host.sin_port = htons(PORT);
    host.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(host_fd, (struct sockaddr*)&host, sizeof(host)) < 0) {
        println("Failed to bind");
        return -1;
    }

    if (listen(host_fd, 128) < 0) {
        println("Failed to listen to socket");
        return -1;
    }

    println("Listening on port {}", PORT);

    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    
    struct pollfd incoming_poll = {
        .fd = host_fd,
        .events = POLLIN,
        .revents = 0,
    };

    while(net_task_running) {
        int num_evt = poll(&incoming_poll, 1, 200);
        if (num_evt == 0) continue;

        for (int i = 0; i < num_evt; ++i) {
            int client_fd = accept(host_fd, (struct sockaddr*)&client, &client_len);

            if (client_fd < 0) {
                println("Failed to accept incoming connection");
            }
            
            if (num_clients >= MAX_CLIENTS) {
                send(client_fd, "Max client reached", 19, 0);
            } else {
                clients[num_clients++] = {
                    .fd = client_fd,
                };
            }

            if (send(client_fd, "Hello friend", 13, 0) < 0) {
                println("Failed to send greetings");
            }

            println("Successfully sent greeting");

        }
   }
    
    return 0;
}

int start_client() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        println("Failed to create socket");
        return -1;
    }

    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(server_fd, (struct sockaddr*)&server, sizeof(server)) < 0) {
        println("Failed to connect to server");
        return -1;
    }

    char buff[2048];
    while(net_task_running) {
        int msg_len = recv(server_fd, &buff, sizeof(buff), 0);
        if (msg_len < 0) {
            println("Failed to read from server");
            return -1;
        }

        GameStatePayload received_state;
        memcpy(&received_state, buff, msg_len);
        on_state_received(received_state);
    }
   
    return 0;
}

void send_game_state(const GameStatePayload *game_state) {
    assert(game_state != nullptr);

    for (int i = 0; i < num_clients; ++i) {
        int fd = clients[i].fd;

        if (send(fd, game_state, sizeof(*game_state), 0) < 0) {
            println("Failed to send game state to client {}", i);
        }
    }
}
