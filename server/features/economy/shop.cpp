#include "shop.h"
#include "../../game_engine.h"

bool buyUpgrade(GameEngine& game, int playerId, const std::string& upgrade) {
    if (game.state != GameState::WAVE_CLEAR) return false;
    if (playerId < 0 || playerId >= GameEngine::MAX_PLAYERS || !game.playerActive[playerId]) return false;
    Tank& p = game.players[playerId];

    if (upgrade == "damage") {
        if (p.bulletDmgLevel >= 3) return false;
        int cost = p.bulletDmgLevel == 1 ? 20 : 50;
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.bulletDmgLevel++;
        p.bulletDamage = p.bulletDmgLevel;
        return true;
    } else if (upgrade == "size") {
        if (p.bulletSizeLevel >= 3) return false;
        int cost = p.bulletSizeLevel == 1 ? 15 : 35;
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.bulletSizeLevel++;
        p.bulletRadius = 3 + (p.bulletSizeLevel - 1) * 2;
        return true;
    } else if (upgrade == "armor") {
        if (p.armorLevel >= 3) return false;
        int cost = p.armorLevel == 1 ? 25 : 60;
        if (game.money[playerId] < cost) return false;
        game.money[playerId] -= cost;
        p.armorLevel++;
        // Max lives table: [normal, hardmode] x [L1, L2, L3]
        // Normal: 4 → 6 → 8 (~200% → 300% → 400%)
        // Hardmode: 2 → 3 → 4
        static const int livesTable[2][3] = { {4, 6, 8}, {2, 3, 4} };
        int mode = game.hardmode ? 1 : 0;
        int newMax = livesTable[mode][p.armorLevel - 1];
        int oldMax = livesTable[mode][p.armorLevel - 2];
        p.lives += (newMax - oldMax);
        return true;
    }
    return false;
}
