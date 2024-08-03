#pragma once
#include "net.h"

void run_game(bool host_mode = false);
void on_state_received(GameStatePayload s);
