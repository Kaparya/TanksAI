#include "collision.h"
#include "../../game_engine.h"

bool rectCollide(float ax, float ay, float as, float bx, float by, float bs) {
    return ax < bx + bs && ax + as > bx && ay < by + bs && ay + as > by;
}

bool wallAt(const int walls[][COLS], float px, float py, float size) {
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

bool tankCollide(const GameEngine& game, const Tank* self, float nx, float ny, float size) {
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (!game.playerActive[i]) continue;
        const Tank& p = game.players[i];
        if (&p == self || !p.alive) continue;
        if (rectCollide(nx, ny, size, p.x - 14, p.y - 14, 28)) return true;
    }
    for (const auto& e : game.enemies) {
        if (&e == self || !e.alive) continue;
        if (rectCollide(nx, ny, size, e.x - 14, e.y - 14, 28)) return true;
    }
    return false;
}

void moveTank(GameEngine& game, Tank& tank, int dir, float spd) {
    tank.dir = dir;
    float nx = tank.x + DX[dir] * spd - 14;
    float ny = tank.y + DY[dir] * spd - 14;
    if (!wallAt(game.walls, nx, ny, 28) && !tankCollide(game, &tank, nx, ny, 28)) {
        tank.x += DX[dir] * spd;
        tank.y += DY[dir] * spd;
    }
}
