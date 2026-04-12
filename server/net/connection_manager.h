#pragma once
#include "../types.h"
#include <map>
#include <string>

class GameEngine;
class WebSocketServer;

class ConnectionManager {
public:
    std::map<int, int> fdToPlayerId;

    void onConnect(GameEngine& game, WebSocketServer& ws, int fd);
    void onDisconnect(GameEngine& game, InputState currentInputs[], int fd);
};
