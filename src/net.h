#pragma once

#include "game.h"
#include "raylib.h"
#include <cstddef>
#include <cstdint>
#include <memory>

struct EntityPayload {
    uint16_t id = 0;
    Vector2 pos {0.f, 0.f};
};

struct GameStatePayload {
    uint64_t server_command_frame = 0;
    float player_pos[2] {0.f, 0.f};
    float player_angle = 0.f;
    uint32_t num_entities = 0;
    std::unique_ptr<EntityPayload[]> entities = nullptr;
};

struct SpawnEntityPayload {
    uint64_t command_frame = 0;
    //WARN: ideally the client should warn the server which ID it used to spawn the entity
    // but the server should respond with which actual ID it used and the client should sync to it, this is not handled here
    uint16_t id = 0;
    Vector2 pos = {0.f, 0.f};
    Vector2 dir = {0.f, 0.f};
};

struct Client {
    int fd = -1;
};

struct Host {
    int fd = -1;
};

int run_host();
int run_client();
/*
 * Schedules the net function to stop in a bit. The thread to runs it can be joined to end the program 
 */
void stop_net();

void send_network_message(const struct SpawnEntityPayload& payload);
void dispatch_game_state(const struct GameStatePayload& game_state);
