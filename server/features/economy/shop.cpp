#include "shop.h"
#include "../../game_engine.h"

// Mirrors UPGRADE_COSTS in client/js/ui/shop.js
// Index = current level (1-based); value = cost to reach next level.
// Index 0 unused (no upgrade from level 0).
static const int UPGRADE_COSTS[3][3] = {
    //  Lv1→2  Lv2→3
    {0,  25,  75},  // damage
    {0,  20,  60},  // size
    {0,  250,  1000},  // armor
};
static constexpr int EXPLOSIVE_AMMO_COST = 300;

static const int IDX_DAMAGE = 0;
static const int IDX_SIZE   = 1;
static const int IDX_ARMOR  = 2;

bool buyUpgrade(GameEngine& game, int playerId, const std::string& upgrade) {
    if (game.state != GameState::WAVE_CLEAR) return false;
    if (playerId < 0 || playerId >= GameEngine::MAX_PLAYERS || !game.playerActive[playerId]) return false;
    Tank& p = game.players[playerId];

    if (upgrade == "damage") {
        if (p.bulletDmgLevel >= 3) return false;
        int cost = UPGRADE_COSTS[IDX_DAMAGE][p.bulletDmgLevel];
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.bulletDmgLevel++;
        p.bulletDamage = p.bulletDmgLevel;
        return true;
    } else if (upgrade == "size") {
        if (p.bulletSizeLevel >= 3) return false;
        int cost = UPGRADE_COSTS[IDX_SIZE][p.bulletSizeLevel];
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.bulletSizeLevel++;
        p.bulletRadius = 3 + (p.bulletSizeLevel - 1) * 2;
        return true;
    } else if (upgrade == "armor") {
        if (p.armorLevel >= 3) return false;
        int cost = UPGRADE_COSTS[IDX_ARMOR][p.armorLevel];
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.armorLevel++;
        p.maxHp = p.armorLevel; // Lv1=1 hp, Lv2=2 hp, Lv3=3 hp per life
        p.hp = p.maxHp;         // restore to full on upgrade
        return true;
    } else if (upgrade == "explosive") {
        if (p.explosiveAmmo) return false;
        if (game.money[playerId] < EXPLOSIVE_AMMO_COST) return false;
        game.money[playerId] -= EXPLOSIVE_AMMO_COST;
        p.explosiveAmmo = true;
        return true;
    }
    return false;
}
