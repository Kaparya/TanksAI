#include "game_engine.h"
#include "websocket_server.h"
#include "http_server.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
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

static std::string getClientDir() {
    // Try relative to executable first
    char pathBuf[4096];
    uint32_t size = sizeof(pathBuf);
    if (_NSGetExecutablePath(pathBuf, &size) == 0) {
        char* dir = dirname(pathBuf);
        // Check if ../client exists (when built in build/)
        std::string candidate = std::string(dir) + "/../client";
        if (access((candidate + "/index.html").c_str(), R_OK) == 0) return candidate;
        // Check if ../../client exists (when built in build/server/)
        candidate = std::string(dir) + "/../../client";
        if (access((candidate + "/index.html").c_str(), R_OK) == 0) return candidate;
    }
    // Fallback: current directory
    if (access("client/index.html", R_OK) == 0) return "client";
    if (access("../client/index.html", R_OK) == 0) return "../client";
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
    InputState currentInput;

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

    ws.setOnConnect([](int fd) {
        printf("[WS] Client connected: %d\n", fd);
    });

    ws.setOnDisconnect([](int fd) {
        printf("[WS] Client disconnected: %d\n", fd);
    });

    ws.setOnMessage([&](int /*fd*/, const std::string& msg) {
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
            // Parse keys object
            auto keysPos = msg.find("\"keys\"");
            if (keysPos != std::string::npos) {
                std::string keysStr = msg.substr(keysPos);
                currentInput.up    = jsonGetBool(keysStr, "up");
                currentInput.down  = jsonGetBool(keysStr, "down");
                currentInput.left  = jsonGetBool(keysStr, "left");
                currentInput.right = jsonGetBool(keysStr, "right");
                currentInput.shoot = jsonGetBool(keysStr, "shoot");
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
            game.tick(currentInput);

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
