#include "game_engine.h"
#include "websocket_server.h"
#include "http_server.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <climits>
#include <mach-o/dyld.h>

static volatile bool running = true;
static void sigHandler(int) { running = false; }

// ── Minimal JSON parser for tiny input messages ─────

struct JsonValue {
    std::string str;
    bool boolean = false;
};

static std::string jsonGetString(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    auto end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - pos - 1);
}

static bool jsonGetBool(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\"";
    auto pos = json.find(search);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos);
    if (pos == std::string::npos) return false;
    auto rest = json.substr(pos + 1);
    // Skip whitespace
    size_t i = 0;
    while (i < rest.size() && (rest[i] == ' ' || rest[i] == '\t')) i++;
    return rest.substr(i, 4) == "true";
}

// ── Resolve client/ directory relative to executable ──

static std::string resolvePath(const std::string& path) {
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved)) return resolved;
    return path;
}

static std::string getClientDir() {
    struct Candidate { std::string path; const char* desc; };
    std::vector<Candidate> candidates;

    // Try relative to executable first
    char pathBuf[4096];
    uint32_t size = sizeof(pathBuf);
    if (_NSGetExecutablePath(pathBuf, &size) == 0) {
        char pathCopy[4096];
        strncpy(pathCopy, pathBuf, sizeof(pathCopy));
        char* dir = dirname(pathCopy);
        candidates.push_back({std::string(dir) + "/../client", "relative to executable (../client)"});
        candidates.push_back({std::string(dir) + "/../../client", "relative to executable (../../client)"});
    }
    // Fallback: relative to CWD
    candidates.push_back({"client", "CWD/client"});
    candidates.push_back({"../client", "CWD/../client"});

    for (auto& c : candidates) {
        if (access((c.path + "/index.html").c_str(), R_OK) == 0) {
            printf("Found client files: %s (%s)\n", resolvePath(c.path).c_str(), c.desc);
            return c.path;
        }
    }

    fprintf(stderr, "\n*** WARNING: client/index.html not found! ***\n");
    fprintf(stderr, "The browser will show 404. Searched:\n");
    for (auto& c : candidates) {
        fprintf(stderr, "  - %s (%s)\n", resolvePath(c.path).c_str(), c.desc);
    }
    fprintf(stderr, "Make sure you run the server from the project root directory.\n\n");
    return "client";
}

// ── Main ────────────────────────────────────────────

int main(int argc, char* argv[]) {
    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);
    signal(SIGPIPE, SIG_IGN);

    int port = 9000;
    if (argc > 1) port = std::atoi(argv[1]);

    std::string clientDir = getClientDir();
    printf("=== TANKS Game Server ===\n");
    printf("Static files: %s\n", clientDir.c_str());

    // HTTP server (serves static files + detects WS upgrades)
    HttpServer http;
    http.setStaticDir(clientDir);
    if (!http.listen(port)) {
        fprintf(stderr, "Failed to bind HTTP on port %d\n", port);
        return 1;
    }
    printf("Listening on http://localhost:%d\n", port);

    // WebSocket server (uses fds handed off from HTTP server)
    WebSocketServer ws;
    GameEngine game;
    InputState currentInputs[GameEngine::MAX_PLAYERS];
    std::map<int, int> fdToPlayerId;

    // When HTTP detects a WS upgrade, hand the fd to the WS server
    http.setOnUpgrade([](int /*fd*/, const std::string& /*data*/) {
        // WS runs on its own port, so upgrades on HTTP port are ignored.
    });

    // Simpler approach: WS on port+1
    if (!ws.listen(port + 1)) {
        fprintf(stderr, "Failed to bind WebSocket on port %d\n", port + 1);
        return 1;
    }
    printf("WebSocket on ws://localhost:%d\n", port + 1);

    ws.setOnConnect([&](int fd) {
        int playerId = game.addPlayer();
        if (playerId < 0) {
            ws.send(fd, "{\"type\":\"full\"}");
            printf("[WS] Client %d rejected — server full\n", fd);
            return;
        }
        fdToPlayerId[fd] = playerId;
        std::string welcome = "{\"type\":\"welcome\",\"playerId\":" + std::to_string(playerId) + "}";
        ws.send(fd, welcome);
        printf("[WS] Client %d connected as player %d\n", fd, playerId);
    });

    ws.setOnDisconnect([&](int fd) {
        auto it = fdToPlayerId.find(fd);
        if (it != fdToPlayerId.end()) {
            int playerId = it->second;
            game.removePlayer(playerId);
            std::memset(&currentInputs[playerId], 0, sizeof(InputState));
            fdToPlayerId.erase(it);
            printf("[WS] Client %d (player %d) disconnected\n", fd, playerId);
        }
    });

    ws.setOnMessage([&](int fd, const std::string& msg) {
        std::string type = jsonGetString(msg, "type");

        if (type == "start") {
            bool hard = jsonGetBool(msg, "hardmode");
            game.start(hard);
            printf("[Game] Started (hardmode=%d)\n", hard);
        } else if (type == "pause") {
            game.pause();
        } else if (type == "resume") {
            game.resume();
        } else if (type == "restart") {
            game.restart();
        } else if (type == "quit") {
            game.quit();
        } else if (type == "input") {
            auto it = fdToPlayerId.find(fd);
            if (it == fdToPlayerId.end()) return;
            int playerId = it->second;
            auto keysPos = msg.find("\"keys\"");
            if (keysPos != std::string::npos) {
                std::string keysStr = msg.substr(keysPos);
                currentInputs[playerId].up    = jsonGetBool(keysStr, "up");
                currentInputs[playerId].down  = jsonGetBool(keysStr, "down");
                currentInputs[playerId].left  = jsonGetBool(keysStr, "left");
                currentInputs[playerId].right = jsonGetBool(keysStr, "right");
                currentInputs[playerId].shoot = jsonGetBool(keysStr, "shoot");
            }
        }
    });

    printf("\nOpen http://localhost:%d in your browser to play!\n\n", port);

    // ── Game loop: 60 FPS ──
    using clock = std::chrono::steady_clock;
    auto frameTime = std::chrono::microseconds(16667); // ~60 FPS
    auto nextTick = clock::now();

    while (running) {
        auto now = clock::now();
        if (now >= nextTick) {
            // Network I/O
            http.poll(0);
            ws.poll(0);

            // Game tick
            game.tick(currentInputs);

            // Send state to all clients
            if (ws.hasClients()) {
                std::string state = game.serializeState();
                ws.broadcast(state);
            }

            nextTick += frameTime;
            // Prevent spiral of death
            if (clock::now() > nextTick + frameTime * 3) {
                nextTick = clock::now();
            }
        } else {
            // Sleep until next tick
            auto sleepTime = std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - now);
            if (sleepTime.count() > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    printf("\nShutting down...\n");
    ws.shutdown();
    http.shutdown();
    return 0;
}
