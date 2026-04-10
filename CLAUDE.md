# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Build and run (preferred)
./scripts/run.sh

# Manual build
mkdir -p build && cd build && cmake .. && make -j$(sysctl -n hw.ncpu) && cd ..

# Run server (HTTP on 9000, WebSocket on 9001)
./build/tanks_server          # default ports
./build/tanks_server 8080     # custom port (HTTP 8080, WS 8081)
```

The server must be run from the project root so it can find `client/`.

## Tests

Tests require a running server and the `websockets` Python package (in `.venv`):

```bash
# Start server first, then in another terminal:
source .venv/bin/activate
python tests/test_multiplayer.py
```

## Architecture

C++ server runs all game logic at 60 FPS and streams full state to browser clients over WebSocket. The client is vanilla JS (no build step) handling only rendering and input.

**Server** (`server/`): Single-threaded game loop in `main.cpp` using non-blocking I/O.
- `game_engine.h/.cpp` -- all game logic: movement, collision, AI, waves, particles, serialization
- `websocket_server.h/.cpp` -- RFC 6455 WebSocket from scratch (SHA-1 + Base64, no deps)
- `http_server.h/.cpp` -- static file server for `client/`

**Client** (`client/`): Plain JS modules loaded via `<script>` tags (no bundler).
- `network.js` -- WebSocket client with auto-reconnect
- `renderer.js` -- Canvas 2D rendering (tanks, bullets, walls, particles, minimap)
- `input.js` -- keyboard capture (WASD/arrows, Space, Esc)
- `ui.js` -- menu, HUD, pause, game-over overlays
- `app.js` -- ties modules together, render loop, state dispatch

## Protocol

All messages are JSON over WebSocket (port = HTTP port + 1).

**Client to server:**
- `{"type":"input","keys":{"up":bool,"down":bool,"left":bool,"right":bool,"shoot":bool}}`
- `{"type":"start","hardmode":bool}`, `{"type":"pause|resume|restart|quit"}`

**Server to client:**
- `{"type":"welcome","playerId":0|1}` on connect
- `{"type":"full"}` when 2 players already connected
- `{"type":"state",...}` every frame: full game state (players, enemies, bullets, walls, particles, score, wave, gameState)

## Game Engine Essentials

- **Map**: 20x15 tile grid, 40px tiles (800x600). Wall values: 0=empty, 1=brick (destructible), 2=steel.
- **Players**: Max 2 co-op. Player 0 (green) spawns bottom-left, Player 1 (cyan) bottom-right. Friendly fire enabled.
- **Waves**: Each wave spawns `4 + 2*wave` enemies with increasing difficulty. New map generated between waves.
- **Directions**: 0=up, 1=right, 2=down, 3=left. Movement via `DX[]`/`DY[]` lookup arrays.
- **Game states**: `MENU`, `PLAYING`, `PAUSED`, `GAMEOVER`.

## Dependencies

Zero external C++ dependencies -- POSIX sockets, standard library only. macOS-specific: uses `_NSGetExecutablePath` for client directory resolution.

Client uses Google Fonts (Orbitron, Share Tech Mono) via CDN. No npm or build tooling.
