#pragma once
#include "../../types.h"

class GameEngine;

void initPlayer(GameEngine& game, int playerId);
int addPlayer(GameEngine& game);
void removePlayer(GameEngine& game, int playerId);
int numActivePlayers(const GameEngine& game);
