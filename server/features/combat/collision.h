#pragma once
#include "../../constants.h"
#include "../../types.h"

class GameEngine;

bool rectCollide(float ax, float ay, float as, float bx, float by, float bs);
bool wallAt(const int walls[][COLS], float px, float py, float size);
bool tankCollide(const GameEngine& game, const Tank* self, float nx, float ny, float size);
void moveTank(GameEngine& game, Tank& tank, int dir, float spd);
