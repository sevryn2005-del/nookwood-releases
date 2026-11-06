// 슬라임 잡기 (C + raylib) — 가이드 전체 예제를 합친 완성본
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ---------- 7장: 숫자는 한곳에 ----------
typedef struct {
    float player_speed, attack_range, attack_cooldown;
    float slime_speed, slime_sight, slime_reach, slime_hit_cooldown;
    int player_hp, slime_hp;
} Rules;

Rules rules = {200, 48, 0.4f, 70, 220, 20, 1.0f, 5, 3};

// 간단한 설정 파일 읽기: 한 줄에 "이름 값" (예: slime_speed 90)
void load_rules(const char *path) {
    FILE *f = fopen(path, "r");
    if (f == NULL) return;                 // 파일이 없으면 기본값 그대로
    char name[64];
    float value;
    while (fscanf(f, "%63s %f", name, &value) == 2) {
        if (strcmp(name, "player_speed") == 0) rules.player_speed = value;
        else if (strcmp(name, "slime_speed") == 0) rules.slime_speed = value;
        else if (strcmp(name, "slime_hp") == 0) rules.slime_hp = (int)value;
    }
    fclose(f);
}

// ---------- 6장: 충돌 ----------
Rectangle walls[] = {{200, 150, 32, 180}, {400, 300, 160, 32}};
const int WALL_COUNT = 2;

int hits_wall(float x, float y, float size) {
    Rectangle body = {x - size / 2, y - size / 2, size, size};
    for (int i = 0; i < WALL_COUNT; i++)
        if (CheckCollisionRecs(body, walls[i])) return 1;
    return 0;
}

void move_with_walls(float *x, float *y, float dx, float dy, float size) {
    if (!hits_wall(*x + dx, *y, size)) *x += dx;   // 가로 먼저
    if (!hits_wall(*x, *y + dy, size)) *y += dy;   // 그다음 세로
}

// ---------- 플레이어와 적 ----------
typedef struct {
    float x, y, invincible, cooldown, face_x, face_y;
    int hp;
} Player;

typedef struct {
    float x, y, home_x, home_y, flash, hit_wait;
    int hp, alive;
} Slime;

// ---------- 9장: 손맛 ----------
typedef struct { float x, y, life; int amount; } Popup;
Popup popups[32];

void add_popup(float x, float y, int amount) {
    for (int i = 0; i < 32; i++) {
        if (popups[i].life <= 0) {
            popups[i] = (Popup){x, y - 20, 0.6f, amount};
            return;
        }
    }
}

void hurt_player(Player *p, int amount) {
    if (p->invincible > 0) return;
    p->hp = p->hp - amount < 0 ? 0 : p->hp - amount;
    p->invincible = 0.8f;
    add_popup(p->x, p->y, amount);
}

// ---------- 8장: 적 ----------
void update_slime(Slime *s, Player *p, float dt) {
    s->flash = fmaxf(0, s->flash - dt);
    s->hit_wait = fmaxf(0, s->hit_wait - dt);
    float dx = p->x - s->x, dy = p->y - s->y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < rules.slime_sight && p->hp > 0) {
        if (dist > rules.slime_reach) {
            float step = rules.slime_speed * dt;
            move_with_walls(&s->x, &s->y, dx / dist * step, dy / dist * step, 20);
        } else if (s->hit_wait == 0) {
            hurt_player(p, 1);
            s->hit_wait = rules.slime_hit_cooldown;
        }
    } else {
        float hx = s->home_x - s->x, hy = s->home_y - s->y;
        float home = sqrtf(hx * hx + hy * hy);
        if (home > 4) {
            float step = rules.slime_speed * dt;
            move_with_walls(&s->x, &s->y, hx / home * step, hy / home * step, 20);
        }
    }
}

// ---------- 10장: 인벤토리 ----------
#define BAG_SIZE 20
#define STACK_LIMIT 64
typedef struct { char item[32]; int count; } Slot;   // count == 0 이면 빈 칸
Slot bag[BAG_SIZE];

int add_item(const char *item, int count) {
    for (int i = 0; i < BAG_SIZE && count > 0; i++) {
        if (bag[i].count > 0 && strcmp(bag[i].item, item) == 0) {
            int put = STACK_LIMIT - bag[i].count;
            if (put > count) put = count;
            bag[i].count += put;
            count -= put;
        }
    }
    for (int i = 0; i < BAG_SIZE && count > 0; i++) {
        if (bag[i].count == 0) {
            int put = count < STACK_LIMIT ? count : STACK_LIMIT;
            strncpy(bag[i].item, item, sizeof(bag[i].item) - 1);
            bag[i].item[sizeof(bag[i].item) - 1] = '\0';
            bag[i].count = put;
            count -= put;
        }
    }
    return count;   // 못 넣은 개수
}

// ---------- 11장: 낮과 밤 ----------
#define DAY_LENGTH 120.0f
float daylight(float time_of_day) {
    float t = time_of_day / DAY_LENGTH;
    return 0.65f + 0.35f * cosf((t - 0.5f) * 2 * PI);
}

// ---------- 12장: 시드로 만드는 세계 ----------
#define SEED 20261029u
float hash01(int x, int y, int salt) {
    unsigned int h = (unsigned)x * 73856093u ^ (unsigned)y * 19349663u ^ (unsigned)salt * 83492791u ^ SEED;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}
float smooth(float t) { return t * t * (3 - 2 * t); }
float noise2(float x, float y, float scale, int salt) {
    int gx = (int)floorf(x / scale), gy = (int)floorf(y / scale);
    float fx = smooth(x / scale - gx), fy = smooth(y / scale - gy);
    float top = hash01(gx, gy, salt) + (hash01(gx + 1, gy, salt) - hash01(gx, gy, salt)) * fx;
    float bottom = hash01(gx, gy + 1, salt) + (hash01(gx + 1, gy + 1, salt) - hash01(gx, gy + 1, salt)) * fx;
    return top + (bottom - top) * fy;
}

// ---------- 13장: 저장 ----------
int save_game(const char *path, const Player *p, float time_of_day) {
    char tmp[256], bak[256];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    snprintf(bak, sizeof bak, "%s.bak", path);
    FILE *f = fopen(tmp, "w");
    if (f == NULL) return 0;
    int ok = fprintf(f, "%f %f %d %f\n", p->x, p->y, p->hp, time_of_day) > 0;
    ok = (fclose(f) == 0) && ok;
    if (!ok) return 0;                     // 임시 파일이 망가져도 원래 저장은 그대로
    remove(bak);
    rename(path, bak);                     // 이전 저장은 백업으로 (없으면 실패해도 괜찮다)
    return rename(tmp, path) == 0;
}

// ---------- 4장~: 게임 루프 ----------
int main(void) {
    load_rules("rules.txt");
    InitWindow(640, 480, "Slime Catch");
    SetTargetFPS(60);

    Player player = {320, 240, 0, 0, 0, 1, rules.player_hp};
    Slime slimes[2] = {
        {500, 120, 500, 120, 0, 0, rules.slime_hp, 1},
        {120, 400, 120, 400, 0, 0, rules.slime_hp, 1},
    };
    float time_of_day = 30;

    while (!WindowShouldClose()) {
        float dt = GetFrameTime();

        // 1) 입력
        float mx = (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) - (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT));
        float my = (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN)) - (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP));
        float length = sqrtf(mx * mx + my * my);
        if (length > 0) {
            mx /= length;
            my /= length;
            player.face_x = mx;
            player.face_y = my;
        }
        int attack = IsKeyPressed(KEY_SPACE);
        if (IsKeyPressed(KEY_F5)) save_game("save.txt", &player, time_of_day);

        // 2) 규칙
        player.invincible = fmaxf(0, player.invincible - dt);
        player.cooldown = fmaxf(0, player.cooldown - dt);
        if (player.hp > 0) {
            float step = rules.player_speed * dt;
            move_with_walls(&player.x, &player.y, mx * step, my * step, 24);
        }
        if (attack && player.cooldown == 0 && player.hp > 0) {
            player.cooldown = rules.attack_cooldown;
            for (int i = 0; i < 2; i++) {
                Slime *s = &slimes[i];
                if (!s->alive) continue;
                float dx = s->x - player.x, dy = s->y - player.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist <= rules.attack_range && (dist == 0 || (dx * player.face_x + dy * player.face_y) / dist >= 0.5f)) {
                    s->hp -= 1;
                    s->flash = 0.15f;
                    add_popup(s->x, s->y, 1);
                    if (s->hp <= 0) {
                        s->alive = 0;
                        add_item("slime_jelly", 1);
                    }
                }
            }
        }
        for (int i = 0; i < 2; i++)
            if (slimes[i].alive) update_slime(&slimes[i], &player, dt);
        for (int i = 0; i < 32; i++) {
            if (popups[i].life > 0) {
                popups[i].life -= dt;
                popups[i].y -= 30 * dt;
            }
        }
        time_of_day = fmodf(time_of_day + dt, DAY_LENGTH);

        // 3) 그리기
        float light = daylight(time_of_day);
        BeginDrawing();
        ClearBackground(BLACK);
        for (int ty = 0; ty < 15; ty++) {
            for (int tx = 0; tx < 20; tx++) {
                float n = noise2((float)tx, (float)ty, 8.0f, 1);
                Color c = n < 0.25f ? (Color){60, 110, 170, 255}
                        : n < 0.32f ? (Color){220, 200, 140, 255}
                                    : (Color){80, 140, 80, 255};
                c.r = (unsigned char)(c.r * light);
                c.g = (unsigned char)(c.g * light);
                c.b = (unsigned char)(c.b * light);
                DrawRectangle(tx * 32, ty * 32, 32, 32, c);
            }
        }
        for (int i = 0; i < WALL_COUNT; i++) DrawRectangleRec(walls[i], (Color){90, 80, 70, 255});
        for (int i = 0; i < 2; i++) {
            if (!slimes[i].alive) continue;
            Color c = slimes[i].flash > 0 ? WHITE : (Color){120, 200, 120, 255};
            DrawCircle((int)slimes[i].x, (int)slimes[i].y, 10, c);
        }
        Color body = player.invincible > 0 ? (Color){255, 120, 120, 255} : (Color){240, 220, 160, 255};
        DrawRectangle((int)player.x - 12, (int)player.y - 12, 24, 24, body);
        for (int i = 0; i < 32; i++)
            if (popups[i].life > 0) DrawText(TextFormat("-%d", popups[i].amount), (int)popups[i].x, (int)popups[i].y, 20, (Color){255, 230, 200, 255});
        DrawText(TextFormat("HP %d", player.hp), 8, 8, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
