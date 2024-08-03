#pragma once

#include "game.h"
#include "raylib.h"

struct GameStatePayload {
    float player_pos[2];
    float player_angle;
    Vector2 entities_pos[ENTITY_COUNT];
    bool entities_active[ENTITY_COUNT];
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


void send_game_state(const struct GameStatePayload* game_state);
