#pragma once
#include "constants.h"
#include <string>
#include <vector>

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
    // Bullet upgrades (players only)
    int bulletDamage = 1;
    int bulletRadius = 3;
    int bulletDmgLevel = 1;
    int bulletSizeLevel = 1;
    // Armor upgrade (players only) — level 1 = none, 2 = light, 3 = heavy
    int armorLevel = 1;
    // Explosive ammo (players only) — one-time shop purchase; bullets deal AoE on impact
    bool explosiveAmmo = false;
    // AI fields (enemies only)
    int aiTimer = 0;
    int aiDir = 2;
};

struct Bullet {
    float x, y;
    float vx, vy;
    int owner; // -1 = enemy, 0 = player 0, 1 = player 1
    bool dead = false;
    int damage = 1;
    float radius = 3.f;
    bool explosive = false; // player upgrade: radial damage on detonation
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

enum class GameState { MENU, PLAYING, PAUSED, GAMEOVER, WAVE_CLEAR };
