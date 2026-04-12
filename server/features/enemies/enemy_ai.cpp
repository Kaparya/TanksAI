#include "enemy_ai.h"
#include "../../game_engine.h"
#include "../combat/collision.h"
#include <cstdlib>
#include <cmath>

static float randf() {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

static void shoot(GameEngine& game, Tank& tank, int owner) {
    if (tank.cooldownTimer > 0) return;
    tank.cooldownTimer = tank.cooldown;
    Bullet b;
    b.x = tank.x + DX[tank.dir] * 16;
    b.y = tank.y + DY[tank.dir] * 16;
    b.vx = DX[tank.dir] * tank.bulletSpeed;
    b.vy = DY[tank.dir] * tank.bulletSpeed;
    b.owner = owner;
    b.damage = tank.bulletDamage;
    b.radius = static_cast<float>(tank.bulletRadius);
    game.bullets.push_back(b);
}

void updateEnemyAI(GameEngine& game, Tank& e) {
    // Find closest alive player
    float targetX = e.x, targetY = e.y;
    float bestDist = 1e9f;
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (!game.playerActive[i] || !game.players[i].alive) continue;
        float dx = game.players[i].x - e.x;
        float dy = game.players[i].y - e.y;
        float dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            targetX = game.players[i].x;
            targetY = game.players[i].y;
        }
    }

    e.aiTimer--;
    if (e.aiTimer <= 0) {
        e.aiTimer = 30 + std::rand() % 60;
        float dx = targetX - e.x;
        float dy = targetY - e.y;
        if (randf() < (game.hardmode ? 0.7f : 0.5f)) {
            if (std::abs(dx) > std::abs(dy)) e.aiDir = dx > 0 ? 1 : 3;
            else e.aiDir = dy > 0 ? 2 : 0;
        } else {
            e.aiDir = std::rand() % 4;
        }
    }
    moveTank(game, e, e.aiDir, e.speed);

    float dx = targetX - e.x;
    float dy = targetY - e.y;
    bool shouldShoot = false;
    if (e.dir == 0 && dy < 0 && std::abs(dx) < 30) shouldShoot = true;
    if (e.dir == 2 && dy > 0 && std::abs(dx) < 30) shouldShoot = true;
    if (e.dir == 1 && dx > 0 && std::abs(dy) < 30) shouldShoot = true;
    if (e.dir == 3 && dx < 0 && std::abs(dy) < 30) shouldShoot = true;
    if (shouldShoot || randf() < (game.hardmode ? 0.035f : 0.02f)) shoot(game, e, -1);
}
