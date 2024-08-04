#pragma once
#include <cstdint>

const uint16_t ENTITY_COUNT = 100;

void run_game(bool host_mode = false);
void on_state_received(struct GameStatePayload&& s);
