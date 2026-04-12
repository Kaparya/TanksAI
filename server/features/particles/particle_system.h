#pragma once
#include "../../types.h"
#include <vector>
#include <string>

void spawnExplosion(std::vector<Particle>& particles, float x, float y, const std::string& color, int count);
void updateParticles(std::vector<Particle>& particles);
