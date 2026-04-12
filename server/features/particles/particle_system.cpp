#include "particle_system.h"
#include <cmath>
#include <cstdlib>

static float randf() {
    return static_cast<float>(std::rand()) / RAND_MAX;
}

void spawnExplosion(std::vector<Particle>& particles, float x, float y, const std::string& color, int count) {
    for (int i = 0; i < count; i++) {
        float angle = randf() * 2.0f * M_PI;
        float spd = 1.0f + randf() * 3.0f;
        Particle p;
        p.x = x; p.y = y;
        p.vx = std::cos(angle) * spd;
        p.vy = std::sin(angle) * spd;
        p.life = 20 + static_cast<int>(randf() * 20);
        p.maxLife = 40;
        p.color = color;
        p.size = 2.0f + randf() * 4.0f;
        particles.push_back(p);
    }
}

void updateParticles(std::vector<Particle>& particles) {
    std::vector<Particle> newParticles;
    for (auto& p : particles) {
        p.x += p.vx; p.y += p.vy;
        p.vx *= 0.95f; p.vy *= 0.95f;
        p.life--;
        if (p.life > 0) {
            newParticles.push_back(p);
        }
    }
    particles = std::move(newParticles);
}
