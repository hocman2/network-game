#include "game.h"
#include "raylib.h"
#include <cstdint>
#include <print>
#include <cmath>
#include "net.h"

using namespace std;

const uint16_t WIN_WIDTH = 800;
const uint16_t WIN_HEIGHT = 450;

static bool host_mode = false;

struct Entity {
    bool exists = false;
    Vector2 pos = {};
    int dir_x = 1;
    int dir_y = 1;
};

struct Player {
    Vector2 position = { 400, 225 };
    float angle = 0.f;
};
Player player;
Entity entities[ENTITY_COUNT];

void print_vec2(const Vector2& vec) {
    println("x: {}; y: {}", vec.x, vec.y);
}

void compute_player_triangle(Vector2 buffer[3]) {
    buffer[0].x = player.position.x - 10;
    buffer[0].y = player.position.y - 10;

    buffer[1].x = player.position.x;
    buffer[1].y = player.position.y + 10;

    buffer[2].x = player.position.x + 10;
    buffer[2].y = player.position.y - 10;
}

void rotate_player_triangle(Vector2 player_triangle[3]) {
    for (int i = 0; i < 3; ++i) {
        Vector2 tri = player_triangle[i];
        
        tri.x -= player.position.x;
        tri.y -= player.position.y;

        int x = tri.x, y = tri.y;
        tri.x = x * cosf(player.angle) - y * sinf(player.angle);
        tri.y = y * cosf(player.angle) + x * sinf(player.angle);

        tri.x += player.position.x;
        tri.y += player.position.y;

        player_triangle[i].x = tri.x;
        player_triangle[i].y = tri.y;
    }
}

void draw_game() {
    BeginDrawing();
    {
        ClearBackground(RAYWHITE);

       // Draw entts
        for (int i = 0; i < ENTITY_COUNT; ++i) {

            if (!entities[i].exists) continue;

            Color color = ColorFromHSV(
                                    (float)i * 360.f / (float)ENTITY_COUNT,
                                    (float)i * 0.4f / (float)ENTITY_COUNT + 0.2f,
                                    (float)i * 0.2f / (float)ENTITY_COUNT + 0.8f
            );

            DrawCircle(entities[i].pos.x, entities[i].pos.y, 10.f, color);        
        }

        // Draw player 
        Vector2 player_triangle[3];
        compute_player_triangle(player_triangle);
        rotate_player_triangle(player_triangle);
        Color player_color = host_mode ? RED : DARKGRAY;
        DrawTriangle(player_triangle[0], player_triangle[1], player_triangle[2], player_color);
    }
    EndDrawing();
}

void process_client_inputs() {
    if (IsKeyPressed(KEY_SPACE)) {
        // Spawn an entity
        for (int i = 0; i < ENTITY_COUNT; ++i) {
            if (!entities[i].exists) {
                entities[i].exists = true;
                break;
            }
        }
    }
}

void process_host_inputs() {
    float dt = GetFrameTime();

    if (IsKeyDown(KEY_RIGHT)) {
        player.angle += (3.f * dt);
        while (player.angle > 6.28f) { player.angle = 0.f + (player.angle - 6.28f); }
    } else if (IsKeyDown(KEY_LEFT)) {
        player.angle -= (3.f * dt);
        while (player.angle < 0.f) { player.angle += 6.28f; }
    }

    if (IsKeyDown(KEY_UP)) {
        Vector2 direction = {
            -sinf(player.angle),
            cosf(player.angle)
        };

        player.position.x += ceil((int)(direction.x * 200.f * dt));
        player.position.y += ceil((int)(direction.y * 200.f * dt));

        if (player.position.x > WIN_WIDTH) { player.position.x = WIN_WIDTH; }
        else if (player.position.x < 0) { player.position.x = 0; }

        if (player.position.y > WIN_HEIGHT) { player.position.y = WIN_HEIGHT; }
        else if (player.position.y < 0) { player.position.y = 0; }
    }
}

void try_send_network_packets() {
    static bool sent_packets = false;

    // Only send packets 1/2 time 
    if (!sent_packets) {

        Vector2 entities_pos[ENTITY_COUNT];
        bool entities_active[ENTITY_COUNT];

        for (int i = 0; i < ENTITY_COUNT; ++i) {
            entities_pos[i] = entities[i].pos;
            entities_active[i] = entities[i].exists;
        }

        GameStatePayload game_state = {
            .player_pos = {player.position.x, player.position.y},
            .player_angle = player.angle,
            .entities_pos = {},
            .entities_active = {},
        };

        memcpy(&game_state.entities_pos, entities_pos, sizeof(Vector2) * ENTITY_COUNT);
        memcpy(&game_state.entities_active, entities_active, sizeof(bool) * ENTITY_COUNT);

        send_game_state(&game_state);
    }

    sent_packets = !sent_packets;
}

void on_state_received(GameStatePayload s) {
    player.position = { s.player_pos[0], s.player_pos[1] };
    player.angle = s.player_angle;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        entities[i].pos = s.entities_pos[i];
        entities[i].exists = s.entities_active[i];
    }
}

void run_game(bool hm) {
	host_mode = hm;

    const int screenWidth = WIN_WIDTH;
    const int screenHeight = WIN_HEIGHT;

    const char* win_name = host_mode ? "Host" : "Client";

    InitWindow(screenWidth, screenHeight, win_name);
    SetTargetFPS(60);

    for (int i = 0; i < ENTITY_COUNT; ++i) {
        entities[i].pos.x = GetRandomValue(0, WIN_WIDTH);
        entities[i].pos.y = GetRandomValue(0, WIN_HEIGHT);
        entities[i].dir_x = i % 2 == 0 ? 1 : -1;
        entities[i].dir_y = i % 2 == 0 ? -1 : 1;
    }

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (host_mode) {
            process_host_inputs();
            process_client_inputs();
        } else {
        } 

        for (int i = 0; i < ENTITY_COUNT; ++i) {
            
            if (!entities[i].exists) continue;

            entities[i].pos.x += ceil((int)(entities[i].dir_x * 200.f * dt));
            entities[i].pos.y += ceil((int)(entities[i].dir_y * 200.f * dt));

            if (entities[i].pos.x >= 800 || entities[i].pos.x <= 0) { entities[i].dir_x *= -1; }
            if (entities[i].pos.y >= 450 || entities[i].pos.y <= 0) { entities[i].dir_y *= -1; }           
        }
        
        if (host_mode) {
            try_send_network_packets();
        }

        draw_game();
    }

    CloseWindow();
}
