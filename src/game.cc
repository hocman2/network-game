#include "game.h"
#include "net.h"
#include "raylib.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <print>

using namespace std;

const uint16_t WIN_WIDTH = 800;
const uint16_t WIN_HEIGHT = 450;

struct Entity {
    uint16_t id;
    bool exists = false;
    Vector2 pos = {};
    int dir_x = 1;
    int dir_y = 1;
};

struct Player {
    Vector2 position = {400, 225};
    float angle = 0.f;
};

Player player;
Entity entities[ENTITY_COUNT];

void print_vec2(const Vector2 &vec) { println("x: {}; y: {}", vec.x, vec.y); }

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

void draw_game(bool host_mode) {
    BeginDrawing();
    {
        ClearBackground(RAYWHITE);

        // Draw entts
        for (int i = 0; i < ENTITY_COUNT; ++i) {

            if (!entities[i].exists) continue;

            Color color = ColorFromHSV((float)i * 360.f / (float)ENTITY_COUNT,
                                 (float)i * 0.4f / (float)ENTITY_COUNT + 0.2f,
                                 (float)i * 0.2f / (float)ENTITY_COUNT + 0.8f);

            DrawCircle(entities[i].pos.x, entities[i].pos.y, 10.f, color);
        }

        // Draw player
        Vector2 player_triangle[3];
        compute_player_triangle(player_triangle);
        rotate_player_triangle(player_triangle);
        Color player_color = host_mode ? RED : DARKGRAY;
        DrawTriangle(player_triangle[0], player_triangle[1], player_triangle[2],
                 player_color);

        DrawFPS(10, 10);
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
        while (player.angle > 6.28f) {
            player.angle = 0.f + (player.angle - 6.28f);
        }
    } else if (IsKeyDown(KEY_LEFT)) {
        player.angle -= (3.f * dt);
        while (player.angle < 0.f) {
            player.angle += 6.28f;
        }
    }

    if (IsKeyDown(KEY_UP)) {
        Vector2 direction = {-sinf(player.angle), cosf(player.angle)};

        player.position.x += ceil((int)(direction.x * 200.f * dt));
        player.position.y += ceil((int)(direction.y * 200.f * dt));

        if (player.position.x > WIN_WIDTH) {
            player.position.x = WIN_WIDTH;
        } else if (player.position.x < 0) {
            player.position.x = 0;
        }

        if (player.position.y > WIN_HEIGHT) {
            player.position.y = WIN_HEIGHT;
        } else if (player.position.y < 0) {
            player.position.y = 0;
        }
    }
}

void try_send_network_packets() {
    static bool sent_packets = false;

    // Only send packets 1/2 time
    if (!sent_packets) {
    
        // Redundant probably
        uint32_t num_active_entities = 0;
        for (int i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].exists) ++num_active_entities;
        }
        
        unique_ptr<EntityPayload[]> active_entities(new EntityPayload[num_active_entities]);

        for (int i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].exists) {
                active_entities[i].id = entities[i].id;
                active_entities[i].pos = entities[i].pos;
            }
        }

        GameStatePayload game_state = {
            .player_pos = {player.position.x, player.position.y},
            .player_angle = player.angle,
            .num_entities = num_active_entities,
            .entities = std::move(active_entities),
        };

        send_game_state(game_state);
    }

    sent_packets = !sent_packets;
}

void on_state_received(GameStatePayload &&s) {
    player.position = {s.player_pos[0], s.player_pos[1]};
    player.angle = s.player_angle;
  
    // Unoptimized, ugly, hashmap by id woudl be better
    for (uint32_t i = 0; i < s.num_entities; ++i) {
        EntityPayload& received_entity = s.entities[i];
        for (int j = 0; j < ENTITY_COUNT; ++j) {
            if (entities[j].id != received_entity.id) continue;
            
            // Make alive if it wasn't bcause it sure is now
            entities[j].exists = true;
            entities[j].pos = received_entity.pos;
            break;
        }
    }
}

void common_init() {
    // Both host and clients agree on the same entity ids
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        entities[i].id = i;
    }
}

void host_init() {
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        entities[i].pos.x = GetRandomValue(0, WIN_WIDTH);
        entities[i].pos.y = GetRandomValue(0, WIN_HEIGHT);
        entities[i].dir_x = i % 2 == 0 ? 1 : -1;
        entities[i].dir_y = i % 2 == 0 ? -1 : 1;
    }
}

void host_update(float dt) {
    process_host_inputs();
    process_client_inputs();

    for (int i = 0; i < ENTITY_COUNT; ++i) {

        if (!entities[i].exists) continue;

        entities[i].pos.x += ceil((int)(entities[i].dir_x * 200.f * dt));
        entities[i].pos.y += ceil((int)(entities[i].dir_y * 200.f * dt));

        if (entities[i].pos.x >= 800 || entities[i].pos.x <= 0) {
            entities[i].dir_x *= -1;
        }
        if (entities[i].pos.y >= 450 || entities[i].pos.y <= 0) {
            entities[i].dir_y *= -1;
        }
    }

    try_send_network_packets();
}

void client_update(float _dt) { process_client_inputs(); }

void run_game(bool host_mode) {

    const int screenWidth = WIN_WIDTH;
    const int screenHeight = WIN_HEIGHT;

    const char *win_name = host_mode ? "Host" : "Client";

    InitWindow(screenWidth, screenHeight, win_name);
    SetTargetFPS(60);

    common_init();

    if (host_mode) {
        host_init();
    }

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        if (host_mode) {
            host_update(dt);
        } else {
            client_update(dt);
        }

        draw_game(host_mode);
    }

    CloseWindow();
}
