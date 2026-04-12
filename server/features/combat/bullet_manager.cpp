#include "bullet_manager.h"
#include "collision.h"
#include "../particles/particle_system.h"
#include "../../game_engine.h"
#include <algorithm>

void shootBullet(GameEngine& game, Tank& tank, int owner) {
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

void updateBullets(GameEngine& game) {
    // Bullets — mark dead instead of erasing mid-loop
    for (size_t i = 0; i < game.bullets.size(); i++) {
        auto& b = game.bullets[i];
        if (b.dead) continue;
        b.x += b.vx;
        b.y += b.vy;

        int gx = static_cast<int>(b.x) / TILE;
        int gy = static_cast<int>(b.y) / TILE;
        if (gx < 0 || gx >= COLS || gy < 0 || gy >= ROWS) {
            b.dead = true; continue;
        }
        if (game.walls[gy][gx] > 0) {
            if (game.walls[gy][gx] == 1) {
                game.walls[gy][gx] = 0;
                game.wallsDirty_ = true;
                spawnExplosion(game.particles, gx * TILE + TILE / 2.0f, gy * TILE + TILE / 2.0f, "#a54", 8);
                game.screenShake = 4;
            } else {
                spawnExplosion(game.particles, b.x, b.y, "#99a", 4);
            }
            b.dead = true; continue;
        }

        // Bullet deflection: player bullets destroy enemy bullets on contact
        if (b.owner >= 0) {
            for (size_t j = 0; j < game.bullets.size(); j++) {
                if (j == i) continue;
                auto& other = game.bullets[j];
                if (other.dead || other.owner >= 0) continue;
                if (rectCollide(b.x - b.radius, b.y - b.radius, b.radius * 2,
                                other.x - other.radius, other.y - other.radius, other.radius * 2)) {
                    spawnExplosion(game.particles, (b.x + other.x) / 2, (b.y + other.y) / 2, "#fff", 10);
                    b.dead = true;
                    other.dead = true;
                    break;
                }
            }
            if (b.dead) continue;
        }

        // Player bullet hits enemy
        if (b.owner >= 0) {
            for (auto& e : game.enemies) {
                if (!e.alive) continue;
                if (rectCollide(b.x - b.radius, b.y - b.radius, b.radius * 2, e.x - 14, e.y - 14, 28)) {
                    e.hp -= b.damage;
                    e.flash = 6;
                    if (e.hp <= 0) {
                        e.alive = false;
                        spawnExplosion(game.particles, e.x, e.y, e.color, 25);
                        game.screenShake = 8;
                        int reward = 10 * e.maxHp;
                        game.score += reward;
                        if (b.owner >= 0 && b.owner < GameEngine::MAX_PLAYERS) {
                            game.money[b.owner] += reward;
                        }
                    } else {
                        spawnExplosion(game.particles, b.x, b.y, "#fff", 4);
                    }
                    b.dead = true;
                    break;
                }
            }
            if (b.dead) continue;
        }

        // Bullet hits player (enemy bullets or friendly fire)
        for (int pi = 0; pi < GameEngine::MAX_PLAYERS; pi++) {
            if (!game.playerActive[pi] || !game.players[pi].alive) continue;
            if (game.players[pi].invuln > 0) continue;
            if (b.owner == pi) continue;
            Tank& p = game.players[pi];
            if (rectCollide(b.x - b.radius, b.y - b.radius, b.radius * 2, p.x - 14, p.y - 14, 28)) {
                b.dead = true;
                p.hp--;
                if (p.hp <= 0) {
                    // Lost all HP for this life — respawn
                    p.hp = p.maxHp;
                    spawnExplosion(game.particles, p.x, p.y, p.color, 20);
                    game.screenShake = 12;
                    p.lives--;
                    if (p.lives <= 0) {
                        p.alive = false;
                        bool allDead = true;
                        for (int j = 0; j < GameEngine::MAX_PLAYERS; j++) {
                            if (game.playerActive[j] && game.players[j].lives > 0) {
                                allDead = false;
                                break;
                            }
                        }
                        if (allDead) {
                            game.state = GameState::GAMEOVER;
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
                } else {
                    // Took damage but still has HP — flash without respawn
                    p.flash = 10;
                    p.invuln = 45;
                    spawnExplosion(game.particles, p.x, p.y, p.color, 6);
                    game.screenShake = 5;
                }
                break;
            }
        }
    }
    // Remove dead bullets in one pass
    game.bullets.erase(
        std::remove_if(game.bullets.begin(), game.bullets.end(), [](const Bullet& b) { return b.dead; }),
        game.bullets.end()
    );
}
