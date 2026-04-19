#pragma once
#include "../constants.h"
#include "../types.h"
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <sstream>
#include <algorithm>

class GameEngine {
public:
    static constexpr int MAX_PLAYERS = 2;

    // Map
    int walls[ROWS][COLS];

    // Entities
    Tank players[MAX_PLAYERS];
    bool playerActive[MAX_PLAYERS] = {false, false};
    std::vector<Tank> enemies;
    std::vector<Bullet> bullets;
    std::vector<Particle> particles;

    // Progress
    int score = 0;
    int wave = 1;
    int spawnTimer = 0;
    int enemiesLeft = 0;
    bool hardmode = false;
    int screenShake = 0;
    int frameCount = 0;
    GameState state = GameState::MENU;
    const char* mapTheme = "standard";

    // Economy
    int money[MAX_PLAYERS] = {0, 0};
    int waveClearTimer = 0;

    // Laser beam visuals (one slot per player; ttl>0 means draw segment x0,y0 — x1,y1)
    LaserBeamVisual laserBeam[MAX_PLAYERS];

    // Cached walls JSON (rebuilt only when walls change)
    mutable std::string wallsJson_;
    mutable bool wallsDirty_ = true;

    GameEngine();
    void start(bool hard);
    void pause();
    void resume();
    void restart();
    void quit();
    void tick(const InputState inputs[MAX_PLAYERS]);
    std::string serializeState() const;

    int addPlayer();
    void removePlayer(int playerId);
    int numActivePlayers() const;
    bool buyUpgrade(int playerId, const std::string& upgrade);

private:
    void generateWalls();
    void initPlayer(int playerId);
    bool wallAt(float px, float py, float size) const;
    bool tankCollide(const Tank* self, float nx, float ny, float size) const;
    bool rectCollide(float ax, float ay, float as, float bx, float by, float bs) const;
    void moveTank(Tank& tank, int dir, float spd);
    void shoot(Tank& tank, int owner);
    bool spawnEnemy();
    void updateEnemyAI(Tank& e);
    void spawnExplosion(float x, float y, const std::string& color, int count);
    void advanceWave();
    float randf() const;
};
