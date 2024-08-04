#pragma once
#include "net.h"
#include <cstdint>

const uint16_t ENTITY_COUNT = 100;

void run_game(bool host_mode = false);
void on_state_received(struct GameStatePayload&& s);
void on_entity_spawned(const struct SpawnEntityPayload& p);
