#include "player_manager.h"
#include "../../game_engine.h"

void initPlayer(GameEngine& game, int playerId) {
    Tank& p = game.players[playerId];
    p = Tank{};
    p.id = playerId;
    p.dir = 0;
    p.speed = 2.5f;
    p.bulletSpeed = 5.0f;
    p.cooldown = 15;
    p.hp = 1;
    p.maxHp = 1;
    p.invuln = 90;
    p.alive = true;
    p.lives = game.hardmode ? 1 : 3;

    if (playerId == 0) {
        p.x = 1.5f * TILE;
        p.y = (ROWS - 2.5f) * TILE;
        p.color = "#00e676";    // green
    } else {
        p.x = (COLS - 2.5f) * TILE;
        p.y = (ROWS - 2.5f) * TILE;
        p.color = "#00bcd4";    // cyan
    }
}

int addPlayer(GameEngine& game) {
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (!game.playerActive[i]) {
            game.playerActive[i] = true;
            initPlayer(game, i);
            return i;
        }
    }
    return -1; // full
}

void removePlayer(GameEngine& game, int playerId) {
    if (playerId < 0 || playerId >= GameEngine::MAX_PLAYERS) return;
    game.playerActive[playerId] = false;
    game.players[playerId].alive = false;

    if (game.state == GameState::PLAYING) {
        bool anyActive = false;
        for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
            if (game.playerActive[i]) { anyActive = true; break; }
        }
        if (!anyActive) {
            game.state = GameState::MENU;
        }
    }
}

int numActivePlayers(const GameEngine& game) {
    int count = 0;
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++)
        if (game.playerActive[i]) count++;
    return count;
}
