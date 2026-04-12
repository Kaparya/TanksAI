#include "map_generator.h"
#include <cstring>
#include <cstdlib>
#include <cmath>

static float randf() {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

void generateWalls(int walls[][COLS], bool hardmode, const char*& mapTheme, bool& wallsDirty) {
    wallsDirty = true;
    std::memset(walls, 0, sizeof(int) * ROWS * COLS);
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

    // Random map theme
    static const char* themes[] = {"standard", "snow", "sand", "city"};
    mapTheme = themes[std::rand() % 4];
}
