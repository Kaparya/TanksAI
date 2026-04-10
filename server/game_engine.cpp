#include "game_engine.h"
#include <cstring>
#include <cstdio>

GameEngine::GameEngine() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::memset(walls, 0, sizeof(walls));
}

float GameEngine::randf() const {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

// ── Map generation ──────────────────────────────────

void GameEngine::generateWalls() {
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
    particles.clear();
    score = 0;
    wave = 1;
    spawnTimer = 0;
    enemiesLeft = 4;
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
    // Check against all players
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

// ── Particles ───────────────────────────────────────

void GameEngine::spawnExplosion(float x, float y, const std::string& color, int count) {
    for (int i = 0; i < count; i++) {
        float angle = randf() * 2.0f * M_PI;
        float spd = 1.0f + randf() * 3.0f;
        Particle p;
        p.x = x; p.y = y;
        p.vx = std::cos(angle) * spd;
        p.vy = std::sin(angle) * spd;
        p.life = 20 + static_cast<int>(randf() * 20);
        p.maxLife = 40;
        p.color = color;
        p.size = 2.0f + randf() * 4.0f;
        particles.push_back(p);
    }
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
    // Find closest alive player
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

    // Bullets
    for (int i = static_cast<int>(bullets.size()) - 1; i >= 0; i--) {
        auto& b = bullets[i];
        b.x += b.vx;
        b.y += b.vy;

        int gx = static_cast<int>(b.x) / TILE;
        int gy = static_cast<int>(b.y) / TILE;
        if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) {
            bullets.erase(bullets.begin() + i); continue;
        }
        if (walls[gy][gx] > 0) {
            if (walls[gy][gx] == 1) {
                walls[gy][gx] = 0;
                spawnExplosion(gx * TILE + TILE / 2.0f, gy * TILE + TILE / 2.0f, "#a54", 8);
                screenShake = 4;
            } else {
                spawnExplosion(b.x, b.y, "#99a", 4);
            }
            bullets.erase(bullets.begin() + i); continue;
        }

        // Bullet deflection: player bullets destroy enemy bullets on contact
        if (b.owner >= 0) {
            bool deflected = false;
            for (int j = static_cast<int>(bullets.size()) - 1; j >= 0; j--) {
                if (j == i) continue;
                auto& other = bullets[j];
                if (other.owner >= 0) continue;  // only deflect enemy bullets
                if (rectCollide(b.x - 3, b.y - 3, 6, other.x - 3, other.y - 3, 6)) {
                    spawnExplosion((b.x + other.x) / 2, (b.y + other.y) / 2, "#fff", 10);
                    // Remove both bullets (higher index first to keep indices valid)
                    int hi = std::max(i, j), lo = std::min(i, j);
                    bullets.erase(bullets.begin() + hi);
                    bullets.erase(bullets.begin() + lo);
                    i = lo - 1;  // adjust loop index
                    deflected = true;
                    break;
                }
            }
            if (deflected) continue;
        }

        // Player bullet hits enemy
        if (b.owner >= 0) {
            bool hit = false;
            for (auto& e : enemies) {
                if (!e.alive) continue;
                if (rectCollide(b.x - 3, b.y - 3, 6, e.x - 14, e.y - 14, 28)) {
                    e.hp--;
                    e.flash = 6;
                    if (e.hp <= 0) {
                        e.alive = false;
                        spawnExplosion(e.x, e.y, e.color, 25);
                        screenShake = 8;
                        score += 100 * wave;
                    } else {
                        spawnExplosion(b.x, b.y, "#fff", 4);
                    }
                    bullets.erase(bullets.begin() + i);
                    hit = true; break;
                }
            }
            if (hit) continue;
        }

        // Bullet hits player (enemy bullets or friendly fire)
        {
            bool hit = false;
            for (int pi = 0; pi < MAX_PLAYERS; pi++) {
                if (!playerActive[pi] || !players[pi].alive) continue;
                if (players[pi].invuln > 0) continue;
                if (b.owner == pi) continue;  // can't hit yourself
                Tank& p = players[pi];
                if (rectCollide(b.x - 3, b.y - 3, 6, p.x - 14, p.y - 14, 28)) {
                    bullets.erase(bullets.begin() + i);
                    spawnExplosion(p.x, p.y, p.color, 20);
                    screenShake = 12;
                    p.lives--;
                    if (p.lives <= 0) {
                        p.alive = false;
                        // Check if ALL active players are out of lives
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
                        // Respawn at their spawn point
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
                    hit = true;
                    break;
                }
            }
            if (hit) continue;
        }
    }

    // Spawn enemies / next wave
    int aliveEnemies = 0;
    for (auto& e : enemies) if (e.alive) aliveEnemies++;

    if (aliveEnemies == 0 && enemiesLeft == 0) {
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
            if (p.lives > 0) {
                p.alive = true;
                int maxLives = hardmode ? 1 : 3;
                if (p.lives < maxLives) p.lives++;
            }
        }
        enemies.clear();
        bullets.clear();
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

    // Particles
    for (int i = static_cast<int>(particles.size()) - 1; i >= 0; i--) {
        auto& p = particles[i];
        p.x += p.vx; p.y += p.vy;
        p.vx *= 0.95f; p.vy *= 0.95f;
        p.life--;
        if (p.life <= 0) particles.erase(particles.begin() + i);
    }

    // Clean dead enemies
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const Tank& t) { return !t.alive && true; }),
        enemies.end()
    );
}

// ── JSON serialization ──────────────────────────────

static std::string escStr(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

std::string GameEngine::serializeState() const {
    std::ostringstream o;
    o.precision(2);
    o << std::fixed;

    const char* stateStr = "menu";
    switch (state) {
        case GameState::PLAYING:  stateStr = "playing"; break;
        case GameState::PAUSED:   stateStr = "paused"; break;
        case GameState::GAMEOVER: stateStr = "gameover"; break;
        default: break;
    }

    o << "{\"type\":\"state\",\"gameState\":\"" << stateStr << "\","
      << "\"score\":" << score << ",\"wave\":" << wave
      << ",\"enemiesLeft\":" << enemiesLeft << ",\"screenShake\":" << screenShake
      << ",\"frameCount\":" << frameCount << ",\"hardmode\":" << (hardmode ? "true" : "false");

    // Players
    o << ",\"players\":[";
    bool firstPlayer = true;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!playerActive[i]) continue;
        if (!firstPlayer) o << ",";
        firstPlayer = false;
        const Tank& p = players[i];
        o << "{\"id\":" << p.id
          << ",\"x\":" << p.x << ",\"y\":" << p.y
          << ",\"dir\":" << p.dir << ",\"alive\":" << (p.alive ? "true" : "false")
          << ",\"invuln\":" << p.invuln << ",\"hp\":" << p.hp
          << ",\"lives\":" << p.lives
          << ",\"color\":\"" << escStr(p.color) << "\"}";
    }
    o << "]";

    // Walls
    o << ",\"walls\":[";
    for (int y = 0; y < ROWS; y++) {
        if (y > 0) o << ",";
        o << "[";
        for (int x = 0; x < COLS; x++) {
            if (x > 0) o << ",";
            o << walls[y][x];
        }
        o << "]";
    }
    o << "]";

    // Enemies
    o << ",\"enemies\":[";
    bool first = true;
    for (const auto& e : enemies) {
        if (!first) o << ",";
        first = false;
        o << "{\"x\":" << e.x << ",\"y\":" << e.y << ",\"dir\":" << e.dir
          << ",\"color\":\"" << escStr(e.color) << "\",\"alive\":" << (e.alive ? "true" : "false")
          << ",\"hp\":" << e.hp << ",\"maxHp\":" << e.maxHp << ",\"flash\":" << e.flash << "}";
    }
    o << "]";

    // Bullets
    o << ",\"bullets\":[";
    first = true;
    for (const auto& b : bullets) {
        if (!first) o << ",";
        first = false;
        o << "{\"x\":" << b.x << ",\"y\":" << b.y
          << ",\"vx\":" << b.vx << ",\"vy\":" << b.vy
          << ",\"owner\":" << b.owner << "}";
    }
    o << "]";

    // Particles
    o << ",\"particles\":[";
    first = true;
    for (const auto& p : particles) {
        if (!first) o << ",";
        first = false;
        o << "{\"x\":" << p.x << ",\"y\":" << p.y
          << ",\"vx\":" << p.vx << ",\"vy\":" << p.vy
          << ",\"life\":" << p.life << ",\"maxLife\":" << p.maxLife
          << ",\"color\":\"" << escStr(p.color) << "\",\"size\":" << p.size << "}";
    }
    o << "]";

    o << "}";
    return o.str();
}
