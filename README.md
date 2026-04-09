# TANKS — Battle Arena

A browser-based tank battle game powered by a C++ backend with 2-player co-op multiplayer.

The C++ server runs all game logic (movement, collision, AI, waves, particles) at 60 FPS and streams state to the browser over WebSocket. The client handles rendering and input only — zero game logic in the browser.

## Requirements

- **macOS** (tested on Apple Silicon)
- **CMake** 3.15+
- **C++17** compiler (Xcode Command Line Tools / AppleClang)

## Build & Run

```bash
cd tanks
./scripts/run.sh
```

Or manually:

```bash
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
cd ..
./build/tanks_server
```

The server starts two listeners:
- **http://localhost:9000** — static file server (open this in your browser)
- **ws://localhost:9001** — WebSocket game server

You can pass a custom base port as an argument:

```bash
./build/tanks_server 8080    # HTTP on 8080, WebSocket on 8081
```

## Multiplayer

The game supports up to **2 players** in co-op mode:

- **1 player** — open one browser tab, play solo against bots (classic mode)
- **2 players** — open a second tab (or another browser), the second player automatically joins the game

### How it works

- Player 1 (green) spawns bottom-left, Player 2 (cyan) spawns bottom-right
- Both players fight enemy waves together on the same field
- **Friendly fire** is enabled — players can shoot each other
- Each player has their own lives (3 normal, 1 in hardmode)
- Shared score across both players
- Game over when all players are out of lives
- Player 2 can join or leave mid-game; the remaining player continues solo
- A third connection is rejected (max 2 players)

## How to Play

| Key              | Action |
|------------------|--------|
| W / Arrow Up     | Move up |
| S / Arrow Down   | Move down |
| A / Arrow Left   | Move left |
| D / Arrow Right  | Move right |
| Space            | Shoot |
| Esc              | Pause / Resume |

- Destroy all enemies to advance to the next wave
- Brown brick walls can be destroyed by bullets; grey steel walls cannot
- **Hardmode**: 1 life, faster/tougher enemies, more aggressive AI

## Shutting Down

Press **Ctrl+C** in the terminal where the server is running. The server handles `SIGINT` and `SIGTERM` gracefully — it will close all WebSocket connections and release the ports before exiting.

Alternatively, from another terminal:

```bash
# Find the process
pgrep -f tanks_server

# Send termination signal
pkill -f tanks_server
```

## Project Structure

```
tanks/
├── CMakeLists.txt
├── server/
│   ├── main.cpp                  # Entry point — HTTP + WS servers, 60 FPS game loop
│   ├── game_engine.h / .cpp      # Game logic: tanks, bullets, collision, AI, waves
│   ├── websocket_server.h / .cpp # Minimal RFC 6455 WebSocket (no external deps)
│   └── http_server.h / .cpp      # Static file server for client/
├── client/
│   ├── index.html
│   ├── css/style.css
│   ├── js/
│   │   ├── app.js                # Bootstrap + main loop
│   │   ├── renderer.js           # Canvas rendering
│   │   ├── input.js              # Keyboard capture
│   │   ├── network.js            # WebSocket client
│   │   └── ui.js                 # Menu, HUD, overlays
│   └── favicon.svg
├── tests/
│   └── test_multiplayer.py       # WebSocket protocol tests
└── scripts/
    └── run.sh                    # Build + run convenience script
```

## Architecture

```
┌──────────┐  WebSocket (JSON)  ┌──────────────┐
│  Browser  │ ◄───────────────► │  C++ Server  │
│           │   input → state   │              │
│ renderer  │                   │ game_engine  │
│ input     │                   │ websocket    │
│ ui        │                   │ http_server  │
└──────────┘                    └──────────────┘
```

- **Client → Server**: `{"type":"input","keys":{"up":true,...,"shoot":false}}`
- **Server → Client**: full game state every frame (players, enemies, bullets, walls, particles, score, wave)
- **On connect**: server assigns player ID via `{"type":"welcome","playerId":0}`
- **Server full**: `{"type":"full"}` sent to third+ connections

No external dependencies — the WebSocket handshake (SHA-1 + Base64) is implemented from scratch.
