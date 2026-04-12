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
    }
    return false;
}
