#pragma once

int start_host();
int start_client();
/*
 * Schedules the net function to stop in a bit. The thread to runs it can be joined to end the program 
 */
void stop_net();

struct GameStatePayload {
    float player_pos[2];
    float player_angle;
};

void send_game_state(const GameStatePayload* game_state);

