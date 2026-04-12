#pragma once
#include "../../types.h"

class GameEngine;

bool spawnEnemy(GameEngine& game);
void advanceWave(GameEngine& game);
void checkWaveProgression(GameEngine& game);
