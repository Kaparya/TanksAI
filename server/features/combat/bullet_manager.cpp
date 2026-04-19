#include "bullet_manager.h"
#include "collision.h"
#include "../particles/particle_system.h"
#include "../../game_engine.h"
#include "../../constants.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

constexpr float EXPLOSION_RADIUS = 56.f; // px — AoE radius (matches ~1.4 tiles)

static void loseOneLife(GameEngine& game, int pi) {
    Tank& p = game.players[pi];
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
        p.hp = p.maxHp;
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
}

// `points` HP loss from one explosion burst (invuln applied only after all points)
static void hurtPlayerBurst(GameEngine& game, int pi, int points) {
    if (!game.playerActive[pi] || !game.players[pi].alive) return;
    Tank& p = game.players[pi];
    if (p.invuln > 0) return;

    int left = points;
    while (left > 0 && p.alive) {
        p.hp--;
        left--;
        if (p.hp <= 0) {
            p.hp = p.maxHp;
            loseOneLife(game, pi);
            if (!p.alive) return;
            // respawned with invuln — stop further burst damage to this player
            return;
        }
    }
    if (points > 0 && p.alive) {
        p.flash = 10;
        p.invuln = 45;
        spawnExplosion(game.particles, p.x, p.y, p.color, 6);
        game.screenShake = 5;
    }
}

// Radial damage: enemies take `damage` HP each; players take `damage` separate hits (respects armor lives)
static void applyExplosiveRadius(GameEngine& game, float cx, float cy, int owner, int damage) {
    spawnExplosion(game.particles, cx, cy, "#ff9100", 35);
    spawnExplosion(game.particles, cx, cy, "#ffd740", 20);
    game.screenShake = std::max(game.screenShake, 14);

    const float tankReach = EXPLOSION_RADIUS + 14.f;
    const float tr2 = tankReach * tankReach;

    // Bricks in radius
    int x0 = std::max(0, static_cast<int>(cx - EXPLOSION_RADIUS) / TILE);
    int x1 = std::min(COLS - 1, static_cast<int>(cx + EXPLOSION_RADIUS) / TILE);
    int y0 = std::max(0, static_cast<int>(cy - EXPLOSION_RADIUS) / TILE);
    int y1 = std::min(ROWS - 1, static_cast<int>(cy + EXPLOSION_RADIUS) / TILE);
    for (int ty = y0; ty <= y1; ty++) {
        for (int tx = x0; tx <= x1; tx++) {
            if (game.walls[ty][tx] != 1) continue;
            float tcx = tx * TILE + TILE * 0.5f;
            float tcy = ty * TILE + TILE * 0.5f;
            float dx = tcx - cx, dy = tcy - cy;
            if (dx * dx + dy * dy <= EXPLOSION_RADIUS * EXPLOSION_RADIUS) {
                game.walls[ty][tx] = 0;
                game.wallsDirty_ = true;
                spawnExplosion(game.particles, tcx, tcy, "#a54", 6);
            }
        }
    }

    // Enemies
    for (auto& e : game.enemies) {
        if (!e.alive) continue;
        float dx = e.x - cx, dy = e.y - cy;
        if (dx * dx + dy * dy > tr2) continue;
        e.hp -= damage;
        e.flash = 6;
        if (e.hp <= 0) {
            e.alive = false;
            spawnExplosion(game.particles, e.x, e.y, e.color, 25);
            int reward = 10 * e.maxHp;
            game.score += reward;
            if (owner >= 0 && owner < GameEngine::MAX_PLAYERS) {
                game.money[owner] += reward;
            }
        } else {
            spawnExplosion(game.particles, e.x, e.y, "#fff", 4);
        }
    }

    // Players (no self-damage; respects invulnerability at explosion time)
    for (int pi = 0; pi < GameEngine::MAX_PLAYERS; pi++) {
        if (pi == owner) continue;
        if (!game.playerActive[pi] || !game.players[pi].alive) continue;
        if (game.players[pi].invuln > 0) continue;
        float dx = game.players[pi].x - cx, dy = game.players[pi].y - cy;
        if (dx * dx + dy * dy > tr2) continue;
        hurtPlayerBurst(game, pi, damage);
    }
}

// Squared distance from point to closed segment — for laser vs enemy bullet (circle)
static float distPointSegmentSq(float px, float py, float x0, float y0, float x1, float y1) {
    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f) {
        float ddx = px - x0, ddy = py - y0;
        return ddx * ddx + ddy * ddy;
    }
    float t = ((px - x0) * dx + (py - y0) * dy) / len2;
    if (t < 0.f) t = 0.f;
    else if (t > 1.f) t = 1.f;
    float qx = x0 + t * dx, qy = y0 + t * dy;
    float ddx = px - qx, ddy = py - qy;
    return ddx * ddx + ddy * ddy;
}

static bool laserSegmentHitsBullet(float x0, float y0, float x1, float y1, const Bullet& b) {
    float r = b.radius;
    return distPointSegmentSq(b.x, b.y, x0, y0, x1, y1) <= r * r;
}

static bool laserSegmentHitsTank(float x0, float y0, float x1, float y1, float cx, float cy) {
    const int samples = 40;
    for (int s = 0; s <= samples; s++) {
        float t = s / static_cast<float>(samples);
        float px = x0 + (x1 - x0) * t;
        float py = y0 + (y1 - y0) * t;
        if (px >= cx - 14.f && px <= cx + 14.f && py >= cy - 14.f && py <= cy + 14.f)
            return true;
    }
    return false;
}

static void fireLaserImpl(GameEngine& game, Tank& tank, int owner) {
    if (tank.cooldownTimer > 0) return;
    tank.cooldownTimer = tank.cooldown;

    int d = tank.dir;
    float sx = tank.x + DX[d] * 18.f;
    float sy = tank.y + DY[d] * 18.f;
    float ex = sx, ey = sy;
    const float step = 3.f;
    const float maxLen = 1000.f;

    for (float dist = step; dist < maxLen; dist += step) {
        float px = sx + DX[d] * dist;
        float py = sy + DY[d] * dist;
        if (px < 0.f || px >= static_cast<float>(MAP_W) || py < 0.f || py >= static_cast<float>(MAP_H))
            break;
        int tx = static_cast<int>(px) / TILE;
        int ty = static_cast<int>(py) / TILE;
        int w = game.walls[ty][tx];
        if (w == 2) {
            ex = sx + DX[d] * (dist - step);
            ey = sy + DY[d] * (dist - step);
            break;
        }
        if (w == 1) {
            game.walls[ty][tx] = 0;
            game.wallsDirty_ = true;
            spawnExplosion(game.particles, tx * TILE + TILE * 0.5f, ty * TILE + TILE * 0.5f, "#ff9800", 3);
        }
        ex = px;
        ey = py;
    }

    // Destroy enemy bullets the beam intersects (same idea as player bullet vs enemy bullet)
    for (auto& b : game.bullets) {
        if (b.dead || b.owner >= 0) continue;
        if (!laserSegmentHitsBullet(sx, sy, ex, ey, b)) continue;
        b.dead = true;
        spawnExplosion(game.particles, b.x, b.y, "#fff", 10);
    }

    game.laserBeam[owner].x0 = sx;
    game.laserBeam[owner].y0 = sy;
    game.laserBeam[owner].x1 = ex;
    game.laserBeam[owner].y1 = ey;
    game.laserBeam[owner].ttl = 15;

    spawnExplosion(game.particles, ex, ey, "#00e5ff", 12);
    game.screenShake = std::max(game.screenShake, 7);

    const int dmg = tank.bulletDamage;

    struct EnemyHit {
        size_t idx;
        float along;
    };
    std::vector<EnemyHit> hits;
    for (size_t i = 0; i < game.enemies.size(); i++) {
        if (!game.enemies[i].alive) continue;
        if (!laserSegmentHitsTank(sx, sy, ex, ey, game.enemies[i].x, game.enemies[i].y)) continue;
        float along = (game.enemies[i].x - sx) * DX[d] + (game.enemies[i].y - sy) * DY[d];
        if (along < 0.f) continue;
        hits.push_back({i, along});
    }
    std::sort(hits.begin(), hits.end(), [](const EnemyHit& a, const EnemyHit& b) { return a.along < b.along; });

    for (const auto& h : hits) {
        auto& e = game.enemies[h.idx];
        if (!e.alive) continue;
        e.hp -= dmg;
        e.flash = 6;
        if (e.hp <= 0) {
            e.alive = false;
            spawnExplosion(game.particles, e.x, e.y, e.color, 25);
            game.screenShake = std::max(game.screenShake, 8);
            int reward = 10 * e.maxHp;
            game.score += reward;
            if (owner >= 0 && owner < GameEngine::MAX_PLAYERS) {
                game.money[owner] += reward;
            }
        } else {
            spawnExplosion(game.particles, e.x, e.y, "#fff", 4);
        }
    }

    for (int pi = 0; pi < GameEngine::MAX_PLAYERS; pi++) {
        if (pi == owner) continue;
        if (!game.playerActive[pi] || !game.players[pi].alive) continue;
        if (game.players[pi].invuln > 0) continue;
        if (!laserSegmentHitsTank(sx, sy, ex, ey, game.players[pi].x, game.players[pi].y)) continue;
        hurtPlayerBurst(game, pi, dmg);
    }
}

static void updateLaserBeamsImpl(GameEngine& game) {
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (game.laserBeam[i].ttl > 0)
            game.laserBeam[i].ttl--;
    }
}

} // namespace

void fireLaser(GameEngine& game, Tank& tank, int owner) {
    fireLaserImpl(game, tank, owner);
}

void updateLaserBeams(GameEngine& game) {
    updateLaserBeamsImpl(game);
}

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
    b.explosive = tank.explosiveAmmo;
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
            if (b.owner >= 0 && b.explosive) {
                applyExplosiveRadius(game, b.x, b.y, b.owner, b.damage);
            } else if (game.walls[gy][gx] == 1) {
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
                    float mx = (b.x + other.x) * 0.5f, my = (b.y + other.y) * 0.5f;
                    spawnExplosion(game.particles, mx, my, "#fff", 10);
                    if (b.explosive) {
                        applyExplosiveRadius(game, mx, my, b.owner, b.damage);
                    }
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
                    if (b.explosive) {
                        applyExplosiveRadius(game, e.x, e.y, b.owner, b.damage);
                    } else {
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
                if (b.owner >= 0 && b.explosive) {
                    applyExplosiveRadius(game, p.x, p.y, b.owner, b.damage);
                } else {
                    p.hp--;
                    if (p.hp <= 0) {
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
                        p.flash = 10;
                        p.invuln = 45;
                        spawnExplosion(game.particles, p.x, p.y, p.color, 6);
                        game.screenShake = 5;
                    }
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
