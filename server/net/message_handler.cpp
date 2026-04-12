#include "message_handler.h"
#include "../core/game.h"

std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

bool jsonGetBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto rest = json.substr(pos + 1);
    size_t i = 0;
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) i++;
    return rest.substr(i, 4) == "true";
}

void handleMessage(GameEngine& game, InputState currentInputs[], std::map<int, int>& fdToPlayerId,
                   int fd, const std::string& msg) {
    std::string type = jsonGetString(msg, "type");

    if (type == "start") {
        bool hard = jsonGetBool(msg, "hardmode");
        game.start(hard);
        printf("[Game] Started (hardmode=%d)\n", hard);
    } else if (type == "pause") {
        game.pause();
    } else if (type == "resume") {
        game.resume();
    } else if (type == "restart") {
        game.restart();
    } else if (type == "quit") {
        game.quit();
    } else if (type == "buy") {
        auto it = fdToPlayerId.find(fd);
        if (it == fdToPlayerId.end()) return;
        std::string upgrade = jsonGetString(msg, "upgrade");
        game.buyUpgrade(it->second, upgrade);
    } else if (type == "input") {
        auto it = fdToPlayerId.find(fd);
        if (it == fdToPlayerId.end()) return;
        int playerId = it->second;
        auto keysPos = msg.find("\"keys\"");
        if (keysPos != std::string::npos) {
            std::string keysStr = msg.substr(keysPos);
            currentInputs[playerId].up    = jsonGetBool(keysStr, "up");
            currentInputs[playerId].down  = jsonGetBool(keysStr, "down");
            currentInputs[playerId].left  = jsonGetBool(keysStr, "left");
            currentInputs[playerId].right = jsonGetBool(keysStr, "right");
            currentInputs[playerId].shoot = jsonGetBool(keysStr, "shoot");
        }
    }
}
