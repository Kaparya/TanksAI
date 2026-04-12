#pragma once
#include "../../types.h"

class GameEngine;

void shootBullet(GameEngine& game, Tank& tank, int owner);
void updateBullets(GameEngine& game);
