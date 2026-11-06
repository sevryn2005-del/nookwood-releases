# 슬라임 잡기 (Python + pygame) — 가이드 전체 예제를 합친 완성본
import json
import math
import os
import pygame

# ---------- 7장: 숫자는 파일에서 ----------
DEFAULTS = {"player_speed": 200, "player_hp": 5, "attack_range": 48,
            "attack_cooldown": 0.4, "slime_speed": 70, "slime_hp": 3,
            "slime_sight": 220, "slime_reach": 20, "slime_hit_cooldown": 1.0}

def load_rules(path):
    rules = dict(DEFAULTS)                 # 기본값을 먼저 깔고
    if os.path.exists(path):
        try:
            with open(path, encoding="utf-8") as f:
                rules.update(json.load(f))  # 파일에 있는 값만 덮어쓴다
        except (OSError, ValueError):
            pass                            # 파일이 깨져도 기본값으로 계속
    return rules

RULES = load_rules("rules.json")

# ---------- 6장: 충돌 ----------
def overlaps(a, b):
    # a, b = (x, y, w, h) 사각형. 서로 겹치면 True
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and \
           a[1] < b[1] + b[3] and b[1] < a[1] + a[3]

WALLS = [(200, 150, 32, 180), (400, 300, 160, 32)]

def move_with_walls(x, y, dx, dy, size):
    # x와 y를 따로 움직여 보고, 벽에 걸리는 쪽만 취소한다(벽을 따라 미끄러진다)
    nx = x + dx
    if any(overlaps((nx - size / 2, y - size / 2, size, size), w) for w in WALLS):
        nx = x
    ny = y + dy
    if any(overlaps((nx - size / 2, ny - size / 2, size, size), w) for w in WALLS):
        ny = y
    return nx, ny

# ---------- 8장: 적 ----------
def make_slime(x, y):
    return {"x": x, "y": y, "home_x": x, "home_y": y, "hp": RULES["slime_hp"],
            "flash": 0.0, "hit_wait": 0.0}

def update_slime(s, player, dt):
    s["flash"] = max(0.0, s["flash"] - dt)
    s["hit_wait"] = max(0.0, s["hit_wait"] - dt)
    dx, dy = player["x"] - s["x"], player["y"] - s["y"]
    dist = math.hypot(dx, dy)
    if dist < RULES["slime_sight"] and player["hp"] > 0:
        if dist > RULES["slime_reach"]:                       # 추적
            step = RULES["slime_speed"] * dt
            s["x"], s["y"] = move_with_walls(s["x"], s["y"], dx / dist * step, dy / dist * step, 20)
        elif s["hit_wait"] == 0.0:                            # 공격
            hurt_player(player, 1)
            s["hit_wait"] = RULES["slime_hit_cooldown"]
    else:                                                     # 귀환
        hx, hy = s["home_x"] - s["x"], s["home_y"] - s["y"]
        home = math.hypot(hx, hy)
        if home > 4:
            step = RULES["slime_speed"] * dt
            s["x"], s["y"] = move_with_walls(s["x"], s["y"], hx / home * step, hy / home * step, 20)

# ---------- 9장: 손맛 ----------
popups = []   # 떠오르는 피해 숫자들

def hurt_player(player, amount):
    if player["invincible"] > 0:
        return
    player["hp"] = max(0, player["hp"] - amount)
    player["invincible"] = 0.8
    popups.append({"x": player["x"], "y": player["y"] - 20, "text": f"-{amount}", "life": 0.6})

# ---------- 10장: 인벤토리 ----------
STACK_LIMIT = 64

def add_item(bag, item, count):
    for slot in bag:                                  # 1) 같은 아이템 칸부터 채운다
        if slot and slot["item"] == item and slot["count"] < STACK_LIMIT:
            put = min(STACK_LIMIT - slot["count"], count)
            slot["count"] += put
            count -= put
            if count == 0:
                return 0
    for i, slot in enumerate(bag):                    # 2) 남으면 빈 칸에
        if slot is None:
            put = min(STACK_LIMIT, count)
            bag[i] = {"item": item, "count": put}
            count -= put
            if count == 0:
                return 0
    return count                                      # 3) 못 넣은 개수를 돌려준다

# ---------- 11장: 낮과 밤 ----------
DAY_LENGTH = 120.0   # 하루 120초

def daylight(time_of_day):
    # 0~1: 정오에 1, 자정에 0.3쯤
    t = time_of_day / DAY_LENGTH
    return 0.65 + 0.35 * math.cos((t - 0.5) * 2 * math.pi)

# ---------- 12장: 시드로 만드는 세계 ----------
SEED = 20261029

def hash01(x, y, salt):
    h = (x * 73856093) ^ (y * 19349663) ^ (salt * 83492791) ^ SEED
    h = (h ^ (h >> 13)) * 1274126177
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0

def smooth(t):
    return t * t * (3 - 2 * t)

def noise(x, y, scale, salt):
    gx, gy = math.floor(x / scale), math.floor(y / scale)
    fx, fy = smooth(x / scale - gx), smooth(y / scale - gy)
    top = hash01(gx, gy, salt) + (hash01(gx + 1, gy, salt) - hash01(gx, gy, salt)) * fx
    bottom = hash01(gx, gy + 1, salt) + (hash01(gx + 1, gy + 1, salt) - hash01(gx, gy + 1, salt)) * fx
    return top + (bottom - top) * fy

def ground_at(tx, ty):
    n = noise(tx, ty, 8.0, 1)
    if n < 0.25:
        return "water"
    if n < 0.32:
        return "sand"
    return "grass"

# ---------- 13장: 저장 ----------
def save_game(path, state):
    tmp = path + ".tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(state, f, ensure_ascii=False)
        with open(tmp, encoding="utf-8") as f:
            json.load(f)                       # 다시 읽어 확인
        if os.path.exists(path):
            os.replace(path, path + ".bak")    # 이전 저장은 백업으로
        os.replace(tmp, path)                  # 확인된 임시 파일로 교체
        return True
    except (OSError, ValueError):
        return False                           # 실패해도 원래 저장은 그대로

def load_game(path):
    for candidate in (path, path + ".bak"):
        try:
            with open(candidate, encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            continue
    return None

# ---------- 4장~: 게임 루프 ----------
def main():
    pygame.init()
    screen = pygame.display.set_mode((640, 480))
    clock = pygame.time.Clock()
    font = pygame.font.Font(None, 24)

    player = {"x": 320.0, "y": 240.0, "hp": RULES["player_hp"], "invincible": 0.0,
              "facing": (0.0, 1.0), "cooldown": 0.0}
    slimes = [make_slime(500, 120), make_slime(120, 400)]
    bag = [None] * 20
    time_of_day = 30.0

    running = True
    while running:
        dt = clock.tick(60) / 1000.0

        # 1) 입력
        attack = False
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_SPACE:
                attack = True
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_F5:
                save_game("save.json", {"player": player, "bag": bag, "time": time_of_day})
        keys = pygame.key.get_pressed()
        mx = (keys[pygame.K_d] or keys[pygame.K_RIGHT]) - (keys[pygame.K_a] or keys[pygame.K_LEFT])
        my = (keys[pygame.K_s] or keys[pygame.K_DOWN]) - (keys[pygame.K_w] or keys[pygame.K_UP])
        length = math.hypot(mx, my)
        if length > 0:
            mx, my = mx / length, my / length
            player["facing"] = (mx, my)

        # 2) 규칙 (상태 바꾸기)
        player["invincible"] = max(0.0, player["invincible"] - dt)
        player["cooldown"] = max(0.0, player["cooldown"] - dt)
        if player["hp"] > 0:
            step = RULES["player_speed"] * dt
            player["x"], player["y"] = move_with_walls(player["x"], player["y"], mx * step, my * step, 24)
        if attack and player["cooldown"] == 0.0 and player["hp"] > 0:
            player["cooldown"] = RULES["attack_cooldown"]
            fx, fy = player["facing"]
            for s in slimes:
                dx, dy = s["x"] - player["x"], s["y"] - player["y"]
                dist = math.hypot(dx, dy)
                if dist <= RULES["attack_range"] and (dist == 0 or (dx * fx + dy * fy) / dist >= 0.5):
                    s["hp"] -= 1
                    s["flash"] = 0.15
                    popups.append({"x": s["x"], "y": s["y"] - 20, "text": "-1", "life": 0.6})
        for s in slimes:
            if s["hp"] <= 0:
                add_item(bag, "slime_jelly", 1)
        slimes = [s for s in slimes if s["hp"] > 0]
        for s in slimes:
            update_slime(s, player, dt)
        for p in popups:
            p["life"] -= dt
            p["y"] -= 30 * dt
        popups[:] = [p for p in popups if p["life"] > 0]
        time_of_day = (time_of_day + dt) % DAY_LENGTH

        # 3) 그리기
        light = daylight(time_of_day)
        def shade(c):
            return tuple(int(v * light) for v in c)
        for ty in range(15):
            for tx in range(20):
                kind = ground_at(tx, ty)
                color = {"water": (60, 110, 170), "sand": (220, 200, 140), "grass": (80, 140, 80)}[kind]
                pygame.draw.rect(screen, shade(color), (tx * 32, ty * 32, 32, 32))
        for w in WALLS:
            pygame.draw.rect(screen, shade((90, 80, 70)), w)
        for s in slimes:
            color = (255, 255, 255) if s["flash"] > 0 else (120, 200, 120)
            pygame.draw.circle(screen, color, (int(s["x"]), int(s["y"])), 10)
        body = (255, 120, 120) if player["invincible"] > 0 else (240, 220, 160)
        pygame.draw.rect(screen, body, (player["x"] - 12, player["y"] - 12, 24, 24))
        for p in popups:
            screen.blit(font.render(p["text"], True, (255, 230, 200)), (p["x"], p["y"]))
        jelly = sum(slot["count"] for slot in bag if slot and slot["item"] == "slime_jelly")
        screen.blit(font.render(f"HP {player['hp']}  jelly {jelly}", True, (255, 255, 255)), (8, 8))
        pygame.display.flip()

    pygame.quit()

if __name__ == "__main__":
    main()
