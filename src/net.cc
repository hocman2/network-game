#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <print>
#include <utility>
#include <memory>
#include <sys/_types/_socklen_t.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include "game.h"
#include "net.h"

const uint16_t PORT = 12345;
const uint16_t MAX_CLIENTS = 255;

using namespace std;

static bool net_task_running = true;

static uint16_t num_clients = 0;
static Client clients[MAX_CLIENTS];
static Host host = {};

void stop_net() {
    net_task_running = false;
}

size_t serialize_game_state(char* buff, size_t buff_len, const GameStatePayload& payload) {
    assert(buff_len > sizeof payload && format("Provided buffer length is guaranteed to not fit a minimal game state payload. {} {}", __FILE__, __LINE__).c_str());

    size_t offset = 0;
    memcpy(buff + offset, &payload.server_command_frame, sizeof payload.server_command_frame);
    offset = sizeof payload.server_command_frame;

    memcpy(buff + offset, &payload.player_pos, sizeof payload.player_pos);
    offset += sizeof payload.player_pos;

    memcpy(buff + offset, &payload.player_angle, sizeof payload.player_angle);
    offset += sizeof payload.player_angle;

    memcpy(buff + offset, &payload.num_entities, sizeof payload.num_entities);
    offset += sizeof payload.num_entities;

    assert(buff_len - offset >= sizeof(EntityPayload) * payload.num_entities && format("Provided buffer is too small to fit all entities. {} {}", __FILE__, __LINE__).c_str());

    for (size_t i = 0; i < payload.num_entities; ++i) {
        memcpy(buff + offset, payload.entities.get() + i, sizeof(EntityPayload));
        offset += sizeof(EntityPayload);
    }

    return offset;
}

void deserialize_game_state(char* msg, size_t msg_len, GameStatePayload& game_state) {
    size_t offset = 0;
    memcpy(&game_state.server_command_frame, msg + offset, sizeof game_state.server_command_frame);
    offset += sizeof game_state.server_command_frame;

    memcpy(&game_state.player_pos, msg + offset, sizeof game_state.player_pos);
    offset += sizeof game_state.player_pos;

    memcpy(&game_state.player_angle, msg + offset, sizeof game_state.player_angle);
    offset += sizeof game_state.player_angle;

    memcpy(&game_state.num_entities, msg + offset, sizeof game_state.num_entities);
    offset += sizeof game_state.num_entities;

    assert(msg_len - offset >= sizeof(EntityPayload) * game_state.num_entities && "Provided message is too small to fit all entities."); 

    game_state.entities = unique_ptr<EntityPayload[]>(new EntityPayload[game_state.num_entities]);
    for (size_t i = 0; i < game_state.num_entities; ++i) {
        //WARN: fine for now, will break if we introduce dynamic array data in EntityPayload
        memcpy(game_state.entities.get() + i, msg + offset, sizeof(EntityPayload));
        offset += sizeof(EntityPayload);
    }
}

void deserialize_spawn_entity(char* msg, size_t msg_len, SpawnEntityPayload& payload) {
    memcpy(&payload, msg, msg_len);
}

void poll_new_connections(struct pollfd* incoming_poll, struct pollfd client_evt_listeners[MAX_CLIENTS]) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);

    int num_evt = poll(incoming_poll, 1, 0);
    if (num_evt == 0) return;

    for (int i = 0; i < num_evt; ++i) {
        int client_fd = accept(incoming_poll->fd, (struct sockaddr*)&client, &client_len);

        if (client_fd < 0) {
            println("Failed to accept incoming connection");
            continue;
        }
        
        if (num_clients >= MAX_CLIENTS) {
            // send a message to the client to inform him we are full
        } else {
            clients[num_clients] = {
                .fd = client_fd,
            };

            client_evt_listeners[num_clients++] = {
                .fd = client_fd,
                .events = POLLIN,
                .revents = 0,
            };
        }
    }
}

void poll_client_messages(struct pollfd client_evt_listeners[MAX_CLIENTS]) {
    int n_evts = poll(client_evt_listeners, num_clients, 0);
    if (n_evts == 0) return;

    char buff[2048];
    for (uint16_t i = 0; i < num_clients; ++i) {
    repeat:
        struct pollfd& client_poll = client_evt_listeners[i];

        if (!client_poll.revents) continue;

        if (client_poll.revents == POLLIN) {
            int msg_len = recv(client_poll.fd,  buff, sizeof buff, 0);
            
            SpawnEntityPayload payload;
            deserialize_spawn_entity(buff, msg_len, payload);
            on_entity_spawned(payload);
        } else if (client_poll.revents == POLLOUT) {
            --num_clients;
            // replace last client with this one 
            client_evt_listeners[i] = client_evt_listeners[num_clients+1];
            // repeat this iteration otherwise that moved client won't get processed due to num_clients being decremented
            goto repeat;
        }
    }
}

int run_host() {
 
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

        
    struct pollfd incoming_poll = {
        .fd = host_fd,
        .events = POLLIN,
        .revents = 0,
    };

    struct pollfd client_evt_listeners[MAX_CLIENTS];

    while(net_task_running) {
        poll_new_connections(&incoming_poll, client_evt_listeners);    
        poll_client_messages(client_evt_listeners);
    }
    
    return 0;
}

int run_client() {
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
    
    host = {
        .fd = server_fd,
    };

    char buff[2048];
    while(net_task_running) {
        int msg_len = recv(server_fd, &buff, sizeof(buff), 0);
        if (msg_len < 0) {
            println("Failed to read from server");
            return -1;
        }
        
        GameStatePayload received_state;
        deserialize_game_state(buff, msg_len, received_state);
        on_state_received(std::move(received_state));
    }
   
    return 0;
}

void send_network_message(const SpawnEntityPayload& payload) {
    if (send(host.fd, &payload, sizeof(SpawnEntityPayload), 0) < 0) {
        println("Failed to send message to the server");
    }
}

void dispatch_game_state(const GameStatePayload& game_state) {
    char buff[2048];
    size_t serialized_len = serialize_game_state(buff, sizeof(buff), game_state);

    for (int i = 0; i < num_clients; ++i) {
        int fd = clients[i].fd;

        if (send(fd, buff, serialized_len, 0) < 0) {
            println("Failed to send game state to client {}", i);
        }
    }
}
