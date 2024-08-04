#include "game.h"
#include "net.h"
#include "raylib.h"
#include "raymath.h"
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <print>
#include <queue>

using namespace std;

const uint16_t WIN_WIDTH = 700;
const uint16_t WIN_HEIGHT = 400;
const float PACKET_SEND_INTERVAL_MS = 1.f / 30.f;
const float CF_UPDATE_RATE = 1.f / 60.f;
const size_t MAX_BUFFERED_STATES = 2;

enum class EntityState {No = 0, Ghost, ServerHandled};

struct Entity {
    uint16_t id;
    EntityState state = EntityState::No;
    Vector2 pos = {};
    float dir_x = 0.f;
    float dir_y = 0.f;
};

struct Player {
    Vector2 position = {(int)(WIN_WIDTH/2), (int)(WIN_HEIGHT/2)};
    float angle = 0.f;
};

uint64_t command_frame = 0;

Player player;
Entity entities[ENTITY_COUNT];

queue<GameStatePayload> buffered_states;
mutex buffered_states_mtx;

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
        for (size_t i = 0; i < ENTITY_COUNT; ++i) {

            if (entities[i].state == EntityState::No) continue;

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
#ifdef NO_NET_INTERP
        DrawText("Running without net interpolation", 10, 25, 16, RED);
#endif
    }
    EndDrawing();
}

void process_client_inputs() {
    if (IsKeyPressed(KEY_SPACE)) {
        Vector2 mp = GetMousePosition();
        Vector2 dir = {(float)GetRandomValue(-10, 10), (float)GetRandomValue(-10, 10)};
        dir = Vector2Normalize(dir);

        int first_available_id = -1;
        for (size_t i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].state != EntityState::No) continue;
            entities[i].state = EntityState::Ghost;
            entities[i].pos = mp;
            entities[i].dir_x = dir.x;
            entities[i].dir_y = dir.y;

            first_available_id = entities[i].id;
            break;
        }

        if (first_available_id == -1) return;

        send_network_message({
            .command_frame = command_frame,
            .id = static_cast<uint16_t>(first_available_id),
            .pos = mp,
            .dir = dir,
        });
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

void try_send_network_packets(float dt) {
    static float time_before_sending = PACKET_SEND_INTERVAL_MS;
    time_before_sending -= dt;

    if (time_before_sending <= 0.f) {
    
        // Redundant probably
        uint32_t num_active_entities = 0;
        for (int i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].state == EntityState::ServerHandled) ++num_active_entities;
        }
        
        unique_ptr<EntityPayload[]> active_entities(new EntityPayload[num_active_entities]);

        for (size_t i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].state == EntityState::ServerHandled) {
                active_entities[i].id = entities[i].id;
                active_entities[i].pos = entities[i].pos;
            }
        }

        GameStatePayload game_state = {
            .server_command_frame = command_frame,
            .player_pos = {player.position.x, player.position.y},
            .player_angle = player.angle,
            .num_entities = num_active_entities,
            .entities = std::move(active_entities),
        };

        dispatch_game_state(game_state);
        time_before_sending = PACKET_SEND_INTERVAL_MS;
    }
}

void interp_to_game_state(const GameStatePayload& s, float dt) {
    int num_fr_per_packets = ceil(1.f / (dt / PACKET_SEND_INTERVAL_MS));
    float inv_num_fr_per_packets = 1.f / (float)num_fr_per_packets;

    // resync command frame
    command_frame = s.server_command_frame;

    player.position.x = Lerp(player.position.x, s.player_pos[0], inv_num_fr_per_packets);
    player.position.y = Lerp(player.position.y, s.player_pos[1], inv_num_fr_per_packets);

    // Note that this causes a small visual glitch under certain circumstances because we are not always lerping the shortest path
    player.angle = Lerp(player.angle, s.player_angle, inv_num_fr_per_packets);

    // Unoptimized, ugly, hashmap by id woudl be better
    for (uint32_t i = 0; i < s.num_entities; ++i) {
        EntityPayload& received_entity = s.entities[i];
        for (int j = 0; j < ENTITY_COUNT; ++j) {
            if (entities[j].id != received_entity.id) continue;
           
            if (entities[j].state == EntityState::ServerHandled) {
                entities[j].pos.x = Lerp(entities[j].pos.x, received_entity.pos.x, inv_num_fr_per_packets);
                entities[j].pos.y = Lerp(entities[j].pos.y, received_entity.pos.y, inv_num_fr_per_packets);
            } else {
                entities[j].pos = received_entity.pos;
                entities[j].state = EntityState::ServerHandled;
            }

            break;
        }
    }
}

void apply_game_state(const GameStatePayload& s) {
    command_frame = s.server_command_frame;

    player.position = {s.player_pos[0], s.player_pos[1]};
    player.angle = s.player_angle;

    for (uint32_t i = 0; i < s.num_entities; ++i) {
        EntityPayload& received_entity = s.entities[i];
        for (uint32_t j = 0; j < ENTITY_COUNT; ++j) {
            if (entities[j].id != received_entity.id) continue;

            entities[j].state = EntityState::ServerHandled;
            entities[j].pos = received_entity.pos;
        }
    }
}

void entity_update(Entity& entity, float dt) {
    entity.pos.x += ceil((int)(entity.dir_x * 200.f * dt));
    entity.pos.y += ceil((int)(entity.dir_y * 200.f * dt));

    if (entity.pos.x >= WIN_WIDTH || entity.pos.x <= 0) {
        entity.dir_x *= -1;
    }
    if (entity.pos.y >= WIN_HEIGHT || entity.pos.y <= 0) {
        entity.dir_y *= -1;
    }
}

void on_state_received(GameStatePayload &&s) {
    buffered_states_mtx.lock();

    // Simulation is too late, ditch the oldest state
    if (buffered_states.size() >= MAX_BUFFERED_STATES) {
        buffered_states.pop();
    }

    buffered_states.push(std::move(s));

    buffered_states_mtx.unlock();
}

void on_entity_spawned(const SpawnEntityPayload& p) {
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        if (entities[i].id != p.id) continue;

        entities[i].state = EntityState::ServerHandled;
        entities[i].pos = p.pos;
        entities[i].dir_x = p.dir.x;
        entities[i].dir_y = p.dir.y;

        // Simulate entity to match client's perspective
        int cf_delta = command_frame - p.command_frame;
        entity_update(entities[i], cf_delta * CF_UPDATE_RATE);
        break;
    }
}

void common_init() {
    // Both host and clients agree on the same entity ids
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        entities[i].id = i;
    }
}

void common_update(float dt) {
    static float cf_update_timer = CF_UPDATE_RATE;
    cf_update_timer -= dt;
    if (cf_update_timer <= 0.f) {
        ++command_frame;
        cf_update_timer = CF_UPDATE_RATE;
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
    common_update(dt);

    process_host_inputs();

    for (int i = 0; i < ENTITY_COUNT; ++i) {

        if (entities[i].state == EntityState::No) continue;
        
        entity_update(entities[i], dt);
    }

    try_send_network_packets(dt);
}

void client_update(float dt) { 
    common_update(dt);

    process_client_inputs();

    if (buffered_states.size() > 0) {
        buffered_states_mtx.lock();
        GameStatePayload& target_state = buffered_states.front();
#ifdef NO_NET_INTERP
        apply_game_state(target_state);    
#else
        interp_to_game_state(target_state, dt);
#endif
        buffered_states_mtx.unlock();

        // Client is tasked to update entities that haven't been server acknowledged yet
        for (size_t i = 0; i < ENTITY_COUNT; ++i) {
            if (entities[i].state == EntityState::Ghost) {
                entity_update(entities[i], dt);
            }
        }
    }
}

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
