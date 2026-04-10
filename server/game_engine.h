#pragma once
#include <vector>
#include <string>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <sstream>
#include <algorithm>

constexpr int TILE = 40;
constexpr int COLS = 20;
constexpr int ROWS = 15;
constexpr int MAP_W = 800;
constexpr int MAP_H = 600;

static const int DX[] = {0, 1, 0, -1};
static const int DY[] = {-1, 0, 1, 0};

struct Tank {
    float x = 0, y = 0;
    int dir = 0;           // 0=up,1=right,2=down,3=left
    std::string color;
    float speed = 0;
    float bulletSpeed = 0;
    int cooldown = 0;
    int cooldownTimer = 0;
    bool alive = true;
    int flash = 0;
    int hp = 1;
    int maxHp = 1;
    int invuln = 0;        // player only
    int id = -1;           // -1 for enemies, 0 or 1 for players
    int lives = 0;         // per-player lives
    // AI fields (enemies only)
    int aiTimer = 0;
    int aiDir = 2;
};

struct Bullet {
    float x, y;
    float vx, vy;
    int owner; // -1 = enemy, 0 = player 0, 1 = player 1
    bool dead = false;
};

struct Particle {
    float x, y;
    float vx, vy;
    int life;
    int maxLife;
    std::string color;
    float size;
};

struct InputState {
    bool up = false, down = false, left = false, right = false;
    bool shoot = false;
};

enum class GameState { MENU, PLAYING, PAUSED, GAMEOVER };

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
    float randf() const;
};
