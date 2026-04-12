#pragma once
#include "../../types.h"
#include <string>

class GameEngine;

bool buyUpgrade(GameEngine& game, int playerId, const std::string& upgrade);
