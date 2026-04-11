#include "game_engine.h"
#include <cstring>
#include <cstdio>

GameEngine::GameEngine() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::memset(walls, 0, sizeof(walls));
    stateJson_.reserve(4096);
}

float GameEngine::randf() const {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

// ── Map generation ──────────────────────────────────

void GameEngine::generateWalls() {
    wallsDirty_ = true;
    std::memset(walls, 0, sizeof(walls));
    // Steel border
    for (int x = 0; x < COLS; x++) { walls[0][x] = 2; walls[ROWS-1][x] = 2; }
    for (int y = 0; y < ROWS; y++) { walls[y][0] = 2; walls[y][COLS-1] = 2; }

    int brickCount = hardmode ? 30 : 40;
    for (int i = 0; i < brickCount; i++) {
        int x = 2 + std::rand() % (COLS - 4);
        int y = 2 + std::rand() % (ROWS - 4);
        if (std::abs(x - 10) < 2 && std::abs(y - 7) < 2) continue;
        if (x < 3 && y > ROWS - 5) continue;              // player 0 spawn
        if (x > COLS - 4 && y > ROWS - 5) continue;        // player 1 spawn
        walls[y][x] = randf() < 0.15f ? 2 : 1;
    }
    for (int i = 0; i < 8; i++) {
        int bx = 2 + std::rand() % (COLS - 4);
        int by = 2 + std::rand() % (ROWS - 4);
        for (int dx = 0; dx < 2; dx++) for (int dy = 0; dy < 2; dy++) {
            int nx = bx + dx, ny = by + dy;
            if (nx > 0 && nx < COLS-1 && ny > 0 && ny < ROWS-1) {
                if (nx < 3 && ny > ROWS - 5) continue;         // player 0 spawn
                if (nx > COLS - 4 && ny > ROWS - 5) continue;  // player 1 spawn
                walls[ny][nx] = 1;
            }
        }
    }
}

void GameEngine::initPlayer(int playerId) {
    Tank& p = players[playerId];
    p = Tank{};
    p.id = playerId;
    p.dir = 0;
    p.speed = 2.5f;
    p.bulletSpeed = 5.0f;
    p.cooldown = 15;
    p.hp = 1;
    p.maxHp = 1;
    p.invuln = 90;
    p.alive = true;
    p.lives = hardmode ? 1 : 3;

    if (playerId == 0) {
        p.x = 1.5f * TILE;
        p.y = (ROWS - 2.5f) * TILE;
        p.color = "#00e676";    // green
    } else {
        p.x = (COLS - 2.5f) * TILE;
        p.y = (ROWS - 2.5f) * TILE;
        p.color = "#00bcd4";    // cyan
    }
}

// ── Player management ──────────────────────────────

int GameEngine::addPlayer() {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i]) {
            playerActive[i] = true;
            initPlayer(i);  // always init so serialized data is valid
            return i;
        }
    }
    return -1; // full
}

void GameEngine::removePlayer(int playerId) {
    if (playerId < 0 || playerId >= MAX_PLAYERS) return;
    playerActive[playerId] = false;
    players[playerId].alive = false;

    if (state == GameState::PLAYING) {
        bool anyActive = false;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (playerActive[i]) { anyActive = true; break; }
        }
        if (!anyActive) {
            state = GameState::MENU;
        }
    }
}

int GameEngine::numActivePlayers() const {
    int count = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (playerActive[i]) count++;
    return count;
}

// ── Game lifecycle ──────────────────────────────────

void GameEngine::start(bool hard) {
    hardmode = hard;
    generateWalls();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (playerActive[i]) initPlayer(i);
    }
    enemies.clear();
    bullets.clear();
    explosions.clear();
    score = 0;
    wave = 1;
    spawnTimer = 0;
    enemiesLeft = 4;
    waveClearTimer = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) money[i] = 0;
    screenShake = 0;
    frameCount = 0;
    state = GameState::PLAYING;
}

void GameEngine::pause()   { if (state == GameState::PLAYING) state = GameState::PAUSED; }
void GameEngine::resume()  { if (state == GameState::PAUSED)  state = GameState::PLAYING; }
void GameEngine::quit()    { state = GameState::MENU; }

void GameEngine::restart() {
    start(hardmode);
}

// ── Collision helpers ───────────────────────────────

bool GameEngine::rectCollide(float ax, float ay, float as, float bx, float by, float bs) const {
    return ax < bx + bs && ax + as > bx && ay < by + bs && ay + as > by;
}

bool GameEngine::wallAt(float px, float py, float size) const {
    int x0 = static_cast<int>(px) / TILE;
    int y0 = static_cast<int>(py) / TILE;
    int x1 = static_cast<int>(px + size - 1) / TILE;
    int y1 = static_cast<int>(py + size - 1) / TILE;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        if (y < 0 || y >= ROWS || x < 0 || x >= COLS) return true;
        if (walls[y][x] > 0) return true;
    }
    return false;
}

bool GameEngine::tankCollide(const Tank* self, float nx, float ny, float size) const {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i]) continue;
        const Tank& p = players[i];
        if (&p == self || !p.alive) continue;
        if (rectCollide(nx, ny, size, p.x - 14, p.y - 14, 28)) return true;
    }
    for (const auto& e : enemies) {
        if (&e == self || !e.alive) continue;
        if (rectCollide(nx, ny, size, e.x - 14, e.y - 14, 28)) return true;
    }
    return false;
}

// ── Movement & shooting ─────────────────────────────

void GameEngine::moveTank(Tank& tank, int dir, float spd) {
    tank.dir = dir;
    float nx = tank.x + DX[dir] * spd - 14;
    float ny = tank.y + DY[dir] * spd - 14;
    if (!wallAt(nx, ny, 28) && !tankCollide(&tank, nx, ny, 28)) {
        tank.x += DX[dir] * spd;
        tank.y += DY[dir] * spd;
    }
}

void GameEngine::shoot(Tank& tank, int owner) {
    if (tank.cooldownTimer > 0) return;
    tank.cooldownTimer = tank.cooldown;
    Bullet b;
    b.x = tank.x + DX[tank.dir] * 16;
    b.y = tank.y + DY[tank.dir] * 16;
    b.vx = DX[tank.dir] * tank.bulletSpeed;
    b.vy = DY[tank.dir] * tank.bulletSpeed;
    b.owner = owner;
    bullets.push_back(b);
}

// ── Explosions (sent as events to client) ──────────

void GameEngine::spawnExplosion(float x, float y, const char* color, int count) {
    explosions.push_back({x, y, color, count});
}

// ── Enemy spawning & AI ─────────────────────────────

bool GameEngine::spawnEnemy() {
    struct Spot { float x, y; };
    Spot spots[] = {
        {1.5f * TILE, 1.5f * TILE},
        {(COLS - 2.5f) * TILE, 1.5f * TILE},
        {(COLS / 2.0f) * TILE, 1.5f * TILE},
    };
    auto& spot = spots[std::rand() % 3];
    if (tankCollide(nullptr, spot.x - 14, spot.y - 14, 28)) return false;

    struct EType { const char* color; float speed, bs; int cd, hp; };
    EType types[] = {
        {"#ff5252", 1.2f, 3.5f, 60, 1},
        {"#ff9100", 2.0f, 3.5f, 50, 1},
        {"#448aff", 1.0f, 5.0f, 40, 2},
        {"#e040fb", 1.5f, 4.0f, 35, 3},
    };
    int maxType = wave <= 2 ? 2 : 4;
    int idx = std::rand() % maxType;
    auto& t = types[idx];

    Tank e{};
    e.x = spot.x; e.y = spot.y;
    e.dir = 2;
    e.color = t.color;
    e.speed = t.speed * (hardmode ? 1.3f : 1.0f);
    e.bulletSpeed = t.bs;
    e.cooldown = std::max(20, t.cd - wave * 2);
    e.hp = t.hp + (hardmode ? 1 : 0);
    e.maxHp = e.hp;
    e.alive = true;
    e.aiTimer = 0;
    e.aiDir = 2;

    enemies.push_back(e);
    spawnExplosion(spot.x, spot.y, "#ffd740", 12);
    return true;
}

void GameEngine::updateEnemyAI(Tank& e) {
    float targetX = e.x, targetY = e.y;
    float bestDist = 1e9f;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i] || !players[i].alive) continue;
        float dx = players[i].x - e.x;
        float dy = players[i].y - e.y;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            targetX = players[i].x;
            targetY = players[i].y;
        }
    }

    e.aiTimer--;
    if (e.aiTimer <= 0) {
        e.aiTimer = 30 + std::rand() % 60;
        float dx = targetX - e.x;
        float dy = targetY - e.y;
        if (randf() < (hardmode ? 0.7f : 0.5f)) {
            if (std::abs(dx) > std::abs(dy)) e.aiDir = dx > 0 ? 1 : 3;
            else e.aiDir = dy > 0 ? 2 : 0;
        } else {
            e.aiDir = std::rand() % 4;
        }
    }
    moveTank(e, e.aiDir, e.speed);

    float dx = targetX - e.x;
    float dy = targetY - e.y;
    bool shouldShoot = false;
    if (e.dir == 0 && dy < 0 && std::abs(dx) < 30) shouldShoot = true;
    if (e.dir == 2 && dy > 0 && std::abs(dx) < 30) shouldShoot = true;
    if (e.dir == 1 && dx > 0 && std::abs(dy) < 30) shouldShoot = true;
    if (e.dir == 3 && dx < 0 && std::abs(dy) < 30) shouldShoot = true;
    if (shouldShoot || randf() < (hardmode ? 0.035f : 0.02f)) shoot(e, -1);
}

// ── Main tick ───────────────────────────────────────

void GameEngine::tick(const InputState inputs[MAX_PLAYERS]) {
    // Clear per-frame explosion events
    explosions.clear();

    if (state == GameState::WAVE_CLEAR) {
        waveClearTimer--;
        if (waveClearTimer <= 0) {
            advanceWave();
        }
        return;
    }
    if (state != GameState::PLAYING) return;
    frameCount++;

    // Player input & invuln
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i] || !players[i].alive) continue;
        Tank& p = players[i];
        if (p.invuln > 0) p.invuln--;
        const InputState& input = inputs[i];
        if      (input.up)    moveTank(p, 0, p.speed);
        else if (input.down)  moveTank(p, 2, p.speed);
        else if (input.left)  moveTank(p, 3, p.speed);
        else if (input.right) moveTank(p, 1, p.speed);
        if (input.shoot) shoot(p, i);
        p.cooldownTimer = std::max(0, p.cooldownTimer - 1);
    }
    screenShake = std::max(0, screenShake - 1);

    // Enemies
    for (auto& e : enemies) {
        if (!e.alive) continue;
        e.cooldownTimer = std::max(0, e.cooldownTimer - 1);
        e.flash = std::max(0, e.flash - 1);
        updateEnemyAI(e);
    }

    // Bullets — mark dead instead of erasing mid-loop
    for (size_t i = 0; i < bullets.size(); i++) {
        auto& b = bullets[i];
        if (b.dead) continue;
        b.x += b.vx;
        b.y += b.vy;

        int gx = static_cast<int>(b.x) / TILE;
        int gy = static_cast<int>(b.y) / TILE;
        if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) {
            b.dead = true; continue;
        }
        if (walls[gy][gx] > 0) {
            if (walls[gy][gx] == 1) {
                walls[gy][gx] = 0;
                wallsDirty_ = true;
                spawnExplosion(gx * TILE + TILE / 2.0f, gy * TILE + TILE / 2.0f, "#a54", 8);
                screenShake = 4;
            } else {
                spawnExplosion(b.x, b.y, "#99a", 4);
            }
            b.dead = true; continue;
        }

        // Bullet deflection: player bullets destroy enemy bullets on contact
        if (b.owner >= 0) {
            for (size_t j = 0; j < bullets.size(); j++) {
                if (j == i) continue;
                auto& other = bullets[j];
                if (other.dead || other.owner >= 0) continue;
                if (rectCollide(b.x - 3, b.y - 3, 6, other.x - 3, other.y - 3, 6)) {
                    spawnExplosion((b.x + other.x) / 2, (b.y + other.y) / 2, "#fff", 10);
                    b.dead = true;
                    other.dead = true;
                    break;
                }
            }
            if (b.dead) continue;
        }

        // Player bullet hits enemy
        if (b.owner >= 0) {
            for (auto& e : enemies) {
                if (!e.alive) continue;
                if (rectCollide(b.x - 3, b.y - 3, 6, e.x - 14, e.y - 14, 28)) {
                    e.hp--;
                    e.flash = 6;
                    if (e.hp <= 0) {
                        e.alive = false;
                        spawnExplosion(e.x, e.y, e.color, 25);
                        screenShake = 8;
                        int reward = 100 * wave;
                        score += reward;
                        if (b.owner >= 0 && b.owner < MAX_PLAYERS) {
                            money[b.owner] += reward;
                        }
                    } else {
                        spawnExplosion(b.x, b.y, "#fff", 4);
                    }
                    b.dead = true;
                    break;
                }
            }
            if (b.dead) continue;
        }

        // Bullet hits player (enemy bullets or friendly fire)
        for (int pi = 0; pi < MAX_PLAYERS; pi++) {
            if (!playerActive[pi] || !players[pi].alive) continue;
            if (players[pi].invuln > 0) continue;
            if (b.owner == pi) continue;
            Tank& p = players[pi];
            if (rectCollide(b.x - 3, b.y - 3, 6, p.x - 14, p.y - 14, 28)) {
                b.dead = true;
                spawnExplosion(p.x, p.y, p.color, 20);
                screenShake = 12;
                p.lives--;
                if (p.lives <= 0) {
                    p.alive = false;
                    bool allDead = true;
                    for (int j = 0; j < MAX_PLAYERS; j++) {
                        if (playerActive[j] && players[j].lives > 0) {
                            allDead = false;
                            break;
                        }
                    }
                    if (allDead) {
                        state = GameState::GAMEOVER;
                    }
                } else {
                    if (pi == 0) {
                        p.x = 1.5f * TILE;
                        p.y = (ROWS - 2.5f) * TILE;
                    } else {
                        p.x = (COLS - 2.5f) * TILE;
                        p.y = (ROWS - 2.5f) * TILE;
                    }
                    p.dir = 0;
                    p.invuln = 90;
                }
                break;
            }
        }
    }
    // Remove dead bullets in one pass
    bullets.erase(
        std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return b.dead; }),
        bullets.end()
    );

    // Spawn enemies / next wave
    int aliveEnemies = 0;
    for (auto& e : enemies) if (e.alive) aliveEnemies++;

    if (aliveEnemies == 0 && enemiesLeft == 0) {
        state = GameState::WAVE_CLEAR;
        waveClearTimer = 180;
        enemies.clear();
        bullets.clear();
        return;
    }

    int maxOnScreen = hardmode ? 4 : 3;
    if (enemiesLeft > 0 && aliveEnemies < maxOnScreen) {
        spawnTimer++;
        int spawnDelay = hardmode ? 60 : 90;
        if (spawnTimer > spawnDelay) {
            if (spawnEnemy()) enemiesLeft--;
            spawnTimer = 0;
        }
    }

    // Clean dead enemies
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const Tank& t) { return !t.alive; }),
        enemies.end()
    );
}

// ── Wave advance ───────────────────────────────────

void GameEngine::advanceWave() {
    wave++;
    enemiesLeft = 3 + wave * 2;
    spawnTimer = 0;
    generateWalls();
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i]) continue;
        Tank& p = players[i];
        if (i == 0) {
            p.x = 1.5f * TILE;
            p.y = (ROWS - 2.5f) * TILE;
        } else {
            p.x = (COLS - 2.5f) * TILE;
            p.y = (ROWS - 2.5f) * TILE;
        }
        p.dir = 0;
        p.invuln = 90;
        p.alive = true;
        int maxLives = hardmode ? 1 : 3;
        if (p.lives < maxLives) p.lives++;
    }
    enemies.clear();
    bullets.clear();
    state = GameState::PLAYING;
}

// ── JSON serialization ──────────────────────────────

// Fast float-to-string with 2 decimal places
static void appendFloat(std::string& s, float v) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%.2f", v);
    s.append(buf, n);
}

static void appendInt(std::string& s, int v) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "%d", v);
    s.append(buf, n);
}

const std::string& GameEngine::serializeState() const {
    // Rebuild walls cache only when walls change
    if (wallsDirty_) {
        wallsJson_.clear();
        wallsJson_ += "[";
        for (int y = 0; y < ROWS; y++) {
            if (y > 0) wallsJson_ += ",";
            wallsJson_ += "[";
            for (int x = 0; x < COLS; x++) {
                if (x > 0) wallsJson_ += ",";
                appendInt(wallsJson_, walls[y][x]);
            }
            wallsJson_ += "]";
        }
        wallsJson_ += "]";
        wallsDirty_ = false;
        wallsVersion_++;
    }

    std::string& o = stateJson_;
    o.clear();

    const char* stateStr = "menu";
    switch (state) {
        case GameState::PLAYING:  stateStr = "playing"; break;
        case GameState::PAUSED:   stateStr = "paused"; break;
        case GameState::GAMEOVER:   stateStr = "gameover"; break;
        case GameState::WAVE_CLEAR: stateStr = "wave_clear"; break;
        default: break;
    }

    o += "{\"type\":\"state\",\"gameState\":\"";
    o += stateStr;
    o += "\",\"score\":"; appendInt(o, score);
    o += ",\"wave\":"; appendInt(o, wave);
    o += ",\"enemiesLeft\":"; appendInt(o, enemiesLeft);
    o += ",\"screenShake\":"; appendInt(o, screenShake);
    o += ",\"frameCount\":"; appendInt(o, frameCount);
    o += ",\"hardmode\":"; o += (hardmode ? "true" : "false");
    o += ",\"waveClearTimer\":"; appendInt(o, waveClearTimer);
    o += ",\"wallsVersion\":"; appendInt(o, wallsVersion_);

    // Players
    o += ",\"players\":[";
    bool firstPlayer = true;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i]) continue;
        if (!firstPlayer) o += ",";
        firstPlayer = false;
        const Tank& p = players[i];
        o += "{\"id\":"; appendInt(o, p.id);
        o += ",\"x\":"; appendFloat(o, p.x);
        o += ",\"y\":"; appendFloat(o, p.y);
        o += ",\"dir\":"; appendInt(o, p.dir);
        o += ",\"alive\":"; o += (p.alive ? "true" : "false");
        o += ",\"invuln\":"; appendInt(o, p.invuln);
        o += ",\"hp\":"; appendInt(o, p.hp);
        o += ",\"lives\":"; appendInt(o, p.lives);
        o += ",\"money\":"; appendInt(o, money[i]);
        o += ",\"color\":\""; o += p.color; o += "\"}";
    }
    o += "]";

    // Walls — only send when version changes (client caches)
    o += ",\"walls\":";
    o += wallsJson_;

    // Enemies
    o += ",\"enemies\":[";
    bool first = true;
    for (const auto& e : enemies) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, e.x);
        o += ",\"y\":"; appendFloat(o, e.y);
        o += ",\"dir\":"; appendInt(o, e.dir);
        o += ",\"color\":\""; o += e.color; o += "\"";
        o += ",\"alive\":"; o += (e.alive ? "true" : "false");
        o += ",\"hp\":"; appendInt(o, e.hp);
        o += ",\"maxHp\":"; appendInt(o, e.maxHp);
        o += ",\"flash\":"; appendInt(o, e.flash); o += "}";
    }
    o += "]";

    // Bullets
    o += ",\"bullets\":[";
    first = true;
    for (const auto& b : bullets) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, b.x);
        o += ",\"y\":"; appendFloat(o, b.y);
        o += ",\"vx\":"; appendFloat(o, b.vx);
        o += ",\"vy\":"; appendFloat(o, b.vy);
        o += ",\"owner\":"; appendInt(o, b.owner); o += "}";
    }
    o += "]";

    // Explosions (events for client to spawn particles locally)
    o += ",\"explosions\":[";
    first = true;
    for (const auto& ex : explosions) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, ex.x);
        o += ",\"y\":"; appendFloat(o, ex.y);
        o += ",\"color\":\""; o += ex.color; o += "\"";
        o += ",\"count\":"; appendInt(o, ex.count); o += "}";
    }
    o += "]";

    o += "}";
    return o;
}
