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
    float player_pos[2] {0.f, 0.f};
    float player_angle = 0.f;
    uint32_t num_entities = 0;
    std::unique_ptr<EntityPayload[]> entities = nullptr;
};

struct Client {
    int fd = -1;
};

struct Host {
    int fd = -1;
};

int start_host();
int start_client();
/*
 * Schedules the net function to stop in a bit. The thread to runs it can be joined to end the program 
 */
void stop_net();


void send_game_state(const struct GameStatePayload& game_state);
