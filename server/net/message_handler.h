#pragma once
#include "../types.h"
#include <string>
#include <map>

class GameEngine;

std::string jsonGetString(const std::string& json, const std::string& key);
bool jsonGetBool(const std::string& json, const std::string& key);

void handleMessage(GameEngine& game, InputState currentInputs[], std::map<int, int>& fdToPlayerId,
                   int fd, const std::string& msg);
