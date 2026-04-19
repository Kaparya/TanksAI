#include "game.h"
#include "../features/combat/collision.h"
#include "../features/combat/bullet_manager.h"
#include "../features/particles/particle_system.h"
#include "../features/map/map_generator.h"
#include "../features/players/player_manager.h"
#include "../features/enemies/enemy_ai.h"
#include "../features/enemies/wave_manager.h"
#include "../features/economy/shop.h"
#include "../net/serializer.h"
#include <cstring>
#include <cstdio>

GameEngine::GameEngine() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));
    std::memset(walls, 0, sizeof(walls));
}

float GameEngine::randf() const {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

// ── Delegates to feature modules ───────────────────

void GameEngine::generateWalls() { ::generateWalls(walls, hardmode, mapTheme, wallsDirty_); }
void GameEngine::initPlayer(int playerId) { ::initPlayer(*this, playerId); }
int GameEngine::addPlayer() { return ::addPlayer(*this); }
void GameEngine::removePlayer(int playerId) { ::removePlayer(*this, playerId); }
int GameEngine::numActivePlayers() const { return ::numActivePlayers(*this); }

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
    waveClearTimer = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) money[i] = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) laserBeam[i] = {};
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

// ── Delegates to feature modules (collision/movement/shooting/particles/enemies) ──

bool GameEngine::rectCollide(float ax, float ay, float as, float bx, float by, float bs) const {
    return ::rectCollide(ax, ay, as, bx, by, bs);
}
bool GameEngine::wallAt(float px, float py, float size) const {
    return ::wallAt(walls, px, py, size);
}
bool GameEngine::tankCollide(const Tank* self, float nx, float ny, float size) const {
    return ::tankCollide(*this, self, nx, ny, size);
}
void GameEngine::moveTank(Tank& tank, int dir, float spd) {
    ::moveTank(*this, tank, dir, spd);
}
void GameEngine::shoot(Tank& tank, int owner) {
    if (tank.laserWeapon)
        ::fireLaser(*this, tank, owner);
    else
        ::shootBullet(*this, tank, owner);
}
void GameEngine::spawnExplosion(float x, float y, const std::string& color, int count) {
    ::spawnExplosion(particles, x, y, color, count);
}
bool GameEngine::spawnEnemy() {
    return ::spawnEnemy(*this);
}
void GameEngine::updateEnemyAI(Tank& e) {
    ::updateEnemyAI(*this, e);
}
void GameEngine::advanceWave() {
    ::advanceWave(*this);
}

// ── Main tick ───────────────────────────────────────

void GameEngine::tick(const InputState inputs[MAX_PLAYERS]) {
    if (state == GameState::WAVE_CLEAR) {
        waveClearTimer--;
        updateParticles(particles);
        updateLaserBeams(*this);
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

    // Bullets
    updateBullets(*this);
    updateLaserBeams(*this);

    // Spawn enemies / next wave
    checkWaveProgression(*this);

    // Particles
    updateParticles(particles);

    // Clean dead enemies
    enemies.erase(
        std::remove_if(enemies.begin(), enemies.end(), [](const Tank& t) { return !t.alive; }),
        enemies.end()
    );
}

// ── Shop ────────────────────────────────────────────

bool GameEngine::buyUpgrade(int playerId, const std::string& upgrade) {
    return ::buyUpgrade(*this, playerId, upgrade);
}

// ── JSON serialization (delegates to net/serializer) ──

std::string GameEngine::serializeState() const {
    return ::serializeState(*this);
}
