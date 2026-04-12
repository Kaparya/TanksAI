#include "serializer.h"
#include "../core/game.h"
#include <cstdio>

static std::string escStr(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else out += c;
    }
    return out;
}

static void appendFloat(std::string& s, float v) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%.2f", v);
    s.append(buf, n);
}

static void appendInt(std::string& s, int v) {
    char buf[16];
    int n = snprintf(buf, sizeof(buf), "%d", v);
    s.append(buf, n);
}

std::string serializeState(const GameEngine& game) {
    // Rebuild walls cache only when walls change
    if (game.wallsDirty_) {
        game.wallsJson_.clear();
        game.wallsJson_ += "[";
        for (int y = 0; y < ROWS; y++) {
            if (y > 0) game.wallsJson_ += ",";
            game.wallsJson_ += "[";
            for (int x = 0; x < COLS; x++) {
                if (x > 0) game.wallsJson_ += ",";
                appendInt(game.wallsJson_, game.walls[y][x]);
            }
            game.wallsJson_ += "]";
        }
        game.wallsJson_ += "]";
        game.wallsDirty_ = false;
    }

    std::string o;
    o.reserve(2048 + game.wallsJson_.size());

    const char* stateStr = "menu";
    switch (game.state) {
        case GameState::PLAYING:  stateStr = "playing"; break;
        case GameState::PAUSED:   stateStr = "paused"; break;
        case GameState::GAMEOVER:   stateStr = "gameover"; break;
        case GameState::WAVE_CLEAR: stateStr = "wave_clear"; break;
        default: break;
    }

    o += "{\"type\":\"state\",\"gameState\":\"";
    o += stateStr;
    o += "\",\"score\":"; appendInt(o, game.score);
    o += ",\"wave\":"; appendInt(o, game.wave);
    o += ",\"enemiesLeft\":"; appendInt(o, game.enemiesLeft);
    o += ",\"screenShake\":"; appendInt(o, game.screenShake);
    o += ",\"frameCount\":"; appendInt(o, game.frameCount);
    o += ",\"hardmode\":"; o += (game.hardmode ? "true" : "false");
    o += ",\"waveClearTimer\":"; appendInt(o, game.waveClearTimer);
    o += ",\"mapTheme\":\""; o += game.mapTheme; o += "\"";

    // Players
    o += ",\"players\":[";
    bool firstPlayer = true;
    for (int i = 0; i < GameEngine::MAX_PLAYERS; i++) {
        if (!game.playerActive[i]) continue;
        if (!firstPlayer) o += ",";
        firstPlayer = false;
        const Tank& p = game.players[i];
        o += "{\"id\":"; appendInt(o, p.id);
        o += ",\"x\":"; appendFloat(o, p.x);
        o += ",\"y\":"; appendFloat(o, p.y);
        o += ",\"dir\":"; appendInt(o, p.dir);
        o += ",\"alive\":"; o += (p.alive ? "true" : "false");
        o += ",\"invuln\":"; appendInt(o, p.invuln);
        o += ",\"hp\":"; appendInt(o, p.hp);
        o += ",\"maxHp\":"; appendInt(o, p.maxHp);
        o += ",\"lives\":"; appendInt(o, p.lives);
        o += ",\"money\":"; appendInt(o, game.money[i]);
        o += ",\"bulletDmgLevel\":"; appendInt(o, p.bulletDmgLevel);
        o += ",\"bulletSizeLevel\":"; appendInt(o, p.bulletSizeLevel);
        o += ",\"armorLevel\":"; appendInt(o, p.armorLevel);
        o += ",\"color\":\""; o += escStr(p.color); o += "\"}";
    }
    o += "]";

    // Walls (cached)
    o += ",\"walls\":";
    o += game.wallsJson_;

    // Enemies
    o += ",\"enemies\":[";
    bool first = true;
    for (const auto& e : game.enemies) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, e.x);
        o += ",\"y\":"; appendFloat(o, e.y);
        o += ",\"dir\":"; appendInt(o, e.dir);
        o += ",\"color\":\""; o += escStr(e.color); o += "\"";
        o += ",\"alive\":"; o += (e.alive ? "true" : "false");
        o += ",\"hp\":"; appendInt(o, e.hp);
        o += ",\"maxHp\":"; appendInt(o, e.maxHp);
        o += ",\"flash\":"; appendInt(o, e.flash); o += "}";
    }
    o += "]";

    // Bullets
    o += ",\"bullets\":[";
    first = true;
    for (const auto& b : game.bullets) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, b.x);
        o += ",\"y\":"; appendFloat(o, b.y);
        o += ",\"vx\":"; appendFloat(o, b.vx);
        o += ",\"vy\":"; appendFloat(o, b.vy);
        o += ",\"owner\":"; appendInt(o, b.owner);
        o += ",\"radius\":"; appendFloat(o, b.radius); o += "}";
    }
    o += "]";

    // Particles
    o += ",\"particles\":[";
    first = true;
    for (const auto& p : game.particles) {
        if (!first) o += ",";
        first = false;
        o += "{\"x\":"; appendFloat(o, p.x);
        o += ",\"y\":"; appendFloat(o, p.y);
        o += ",\"vx\":"; appendFloat(o, p.vx);
        o += ",\"vy\":"; appendFloat(o, p.vy);
        o += ",\"life\":"; appendInt(o, p.life);
        o += ",\"maxLife\":"; appendInt(o, p.maxLife);
        o += ",\"color\":\""; o += escStr(p.color); o += "\"";
        o += ",\"size\":"; appendFloat(o, p.size); o += "}";
    }
    o += "]";

    o += "}";
    return o;
}
