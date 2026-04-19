#pragma once
#include "../../types.h"

class GameEngine;

void shootBullet(GameEngine& game, Tank& tank, int owner);
void fireLaser(GameEngine& game, Tank& tank, int owner);
void updateLaserBeams(GameEngine& game);
void updateBullets(GameEngine& game);
