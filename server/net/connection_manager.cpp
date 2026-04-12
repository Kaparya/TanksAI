#include "connection_manager.h"
#include "../core/game.h"
#include "websocket_server.h"
#include <cstdio>
#include <cstring>

void ConnectionManager::onConnect(GameEngine& game, WebSocketServer& ws, int fd) {
    int playerId = game.addPlayer();
    if (playerId < 0) {
        ws.send(fd, "{\"type\":\"full\"}");
        printf("[WS] Client %d rejected — server full\n", fd);
        return;
    }
    fdToPlayerId[fd] = playerId;
    std::string welcome = "{\"type\":\"welcome\",\"playerId\":" + std::to_string(playerId) + "}";
    ws.send(fd, welcome);
    printf("[WS] Client %d connected as player %d\n", fd, playerId);
}

void ConnectionManager::onDisconnect(GameEngine& game, InputState currentInputs[], int fd) {
    auto it = fdToPlayerId.find(fd);
    if (it != fdToPlayerId.end()) {
        int playerId = it->second;
        game.removePlayer(playerId);
        std::memset(&currentInputs[playerId], 0, sizeof(InputState));
        fdToPlayerId.erase(it);
        printf("[WS] Client %d (player %d) disconnected\n", fd, playerId);
    }
}
