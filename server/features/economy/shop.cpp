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
        p.maxHp = p.armorLevel; // Lv1=1 hp, Lv2=2 hp, Lv3=3 hp per life
        p.hp = p.maxHp;         // restore to full on upgrade
        return true;
    }
    return false;
}
