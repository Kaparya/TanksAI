---
name: Tanks server protocol constants and game rules
description: Key numeric constants, spawn positions, and behavioral rules extracted from C++ source
type: project
---

From `game_engine.h` / `game_engine.cpp`:

MAP: 20 cols x 15 rows, 40px tiles = 800x600px
Walls: 0=empty, 1=brick (destructible), 2=steel (indestructible)
Border: entire row 0, row 14, col 0, col 19 are always steel (type 2)

Players:
- P0 spawns at x=60 (1.5*40), y=500 ((15-2.5)*40), dir=0 (up), color=#00e676
- P1 spawns at x=700 ((20-2.5)*40), y=500, dir=0, color=#00bcd4
- speed=2.5 px/tick, bulletSpeed=5.0, cooldown=15 ticks, hp=1, invuln=90 frames
- Normal mode: lives=3. Hard mode: lives=1

Wave 1: enemiesLeft=4, maxOnScreen=3 (normal) / 4 (hard)
Spawn delay: 90 ticks normal / 60 hard (~1.5s / 1s at 60 fps)
Wave N+1 enemiesLeft: 3 + wave*2
Score per kill: 100 * current_wave

Enemy types (wave 1 only uses types 0 and 1):
- #ff5252 red: speed 1.2, cd 60, hp 1
- #ff9100 orange: speed 2.0, cd 50, hp 1
- #448aff blue: speed 1.0, cd 40, hp 2  (wave 3+)
- #e040fb purple: speed 1.5, cd 35, hp 3  (wave 3+)

Protocol:
- Client->Server: {"type":"input","keys":{up,down,left,right,shoot: bool}}
- Client->Server: {"type":"start","hardmode":bool} | pause | resume | restart | quit
- Server->Client: {"type":"welcome","playerId":0|1} on connect
- Server->Client: {"type":"full"} when both slots taken
- Server->Client: {"type":"state",...} every frame at 60fps

State shape: type, gameState("menu"/"playing"/"paused"/"gameover"),
  score, wave, enemiesLeft, screenShake, frameCount, hardmode,
  players[], walls[][], enemies[], bullets[], particles[]
No top-level "lives" field — lives are per-player in players[].

Friendly fire: enabled. Player cannot shoot themselves (owner==pi skipped).
Invuln blocks incoming damage only; does not block shooting.
