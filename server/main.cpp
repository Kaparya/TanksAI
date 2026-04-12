#include "core/game.h"
#include "net/websocket_server.h"
#include "net/http_server.h"
#include "net/connection_manager.h"
#include "net/message_handler.h"
#include "net/serializer.h"
#include <cstdio>
#include <cstring>
#include <chrono>
#include <thread>
#include <string>
#include <signal.h>
#include <unistd.h>
#include <libgen.h>
#include <climits>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static volatile bool running = true;
static void sigHandler(int) { running = false; }

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
    bool gotExePath = false;
#ifdef __APPLE__
    uint32_t size = sizeof(pathBuf);
    gotExePath = (_NSGetExecutablePath(pathBuf, &size) == 0);
#else
    ssize_t len = readlink("/proc/self/exe", pathBuf, sizeof(pathBuf) - 1);
    if (len > 0) { pathBuf[len] = '\0'; gotExePath = true; }
#endif
    if (gotExePath) {
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

    // HTTP server (serves static files)
    HttpServer http;
    http.setStaticDir(clientDir);
    if (!http.listen(port)) {
        fprintf(stderr, "Failed to bind HTTP on port %d\n", port);
        return 1;
    }
    printf("Listening on http://localhost:%d\n", port);

    // WebSocket server on port+1
    WebSocketServer ws;
    GameEngine game;
    InputState currentInputs[GameEngine::MAX_PLAYERS];
    ConnectionManager connMgr;

    http.setOnUpgrade([](int /*fd*/, const std::string& /*data*/) {
        // WS runs on its own port, so upgrades on HTTP port are ignored.
    });

    if (!ws.listen(port + 1)) {
        fprintf(stderr, "Failed to bind WebSocket on port %d\n", port + 1);
        return 1;
    }
    printf("WebSocket on ws://localhost:%d\n", port + 1);

    ws.setOnConnect([&](int fd) {
        connMgr.onConnect(game, ws, fd);
    });

    ws.setOnDisconnect([&](int fd) {
        connMgr.onDisconnect(game, currentInputs, fd);
    });

    ws.setOnMessage([&](int fd, const std::string& msg) {
        handleMessage(game, currentInputs, connMgr.fdToPlayerId, fd, msg);
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
