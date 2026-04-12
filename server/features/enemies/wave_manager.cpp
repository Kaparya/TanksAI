#include "wave_manager.h"
#include "../../game_engine.h"
#include "../combat/collision.h"
#include "../particles/particle_system.h"
#include "../map/map_generator.h"
#include <cstdlib>

static float randf() {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

bool spawnEnemy(GameEngine& game) {
    struct Spot { float x, y; };
    Spot spot;
    while (true) {
        spot = {
            (1.5f + randf() * (COLS - 1.5f)) * TILE,
            (1.5f + randf() * (ROWS - 1.5f)) * TILE
        };
        float tlx = spot.x - 14, tly = spot.y - 14;
        if (!wallAt(game.walls, tlx, tly, 28)
                && !tankCollide(game, nullptr, tlx - 2 * TILE, tly - 2 * TILE, 28 + 4 * TILE)) {
            break;
        }
    }

    struct EType { const char* color; float speed, bs; int cd, hp; };
    EType types[] = {
        {"#ff5252", 1.2f, 3.5f, 60, 1},
        {"#ff9100", 2.0f, 3.5f, 50, 1},
        {"#448aff", 1.0f, 5.0f, 40, 2},
        {"#e040fb", 1.5f, 4.0f, 35, 3},
    };
    int maxType = game.wave <= 2 ? 2 : 4;
    int idx = std::rand() % maxType;
    auto& t = types[idx];

    Tank e{};
    e.x = spot.x; e.y = spot.y;
    e.dir = 2;
    e.color = t.color;
    e.speed = t.speed * (game.hardmode ? 1.3f : 1.0f);
    e.bulletSpeed = t.bs;
    e.cooldown = std::max(20, t.cd - game.wave * 2);
    e.hp = t.hp + (game.hardmode ? 1 : 0);
    e.maxHp = e.hp;
    e.alive = true;
    e.aiTimer = 0;
    e.aiDir = 2;

    game.enemies.push_back(e);
    spawnExplosion(game.particles, spot.x, spot.y, "#ffd740", 12);
    return true;
}

void advanceWave(GameEngine& game) {
    game.wave++;
    game.enemiesLeft = 3 + game.wave * 2;
    game.spawnTimer = 0;
    generateWalls(game.walls, game.hardmode, game.mapTheme, game.wallsDirty_);
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (!game.playerActive[i]) continue;
        Tank& p = game.players[i];
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
        // Max lives respect armor level
        static const int livesTable[2][3] = { {4, 6, 8}, {2, 3, 4} };
        int mode = game.hardmode ? 1 : 0;
        int maxLives = livesTable[mode][p.armorLevel - 1];
        if (p.lives < maxLives) p.lives++;
    }
    game.enemies.clear();
    game.bullets.clear();
    game.state = GameState::PLAYING;
}

void checkWaveProgression(GameEngine& game) {
    int aliveEnemies = 0;
    for (auto& e : game.enemies) if (e.alive) aliveEnemies++;

    if (aliveEnemies == 0 && game.enemiesLeft == 0) {
        game.state = GameState::WAVE_CLEAR;
        game.waveClearTimer = 360;
        game.enemies.clear();
        game.bullets.clear();
        return;
    }

    int maxOnScreen = game.hardmode ? 4 : 3;
    if (game.enemiesLeft > 0 && aliveEnemies < maxOnScreen) {
        game.spawnTimer++;
        int spawnDelay = game.hardmode ? 60 : 90;
        if (game.spawnTimer > spawnDelay) {
            if (spawnEnemy(game)) game.enemiesLeft--;
            game.spawnTimer = 0;
        }
    }
}
