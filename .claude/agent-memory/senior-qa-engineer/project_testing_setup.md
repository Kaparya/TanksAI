---
name: Tanks project testing setup
description: Testing framework, test directory, run commands, and file structure for the tanks game
type: project
---

Testing framework: Python asyncio + websockets library (no pytest, raw asyncio.run).

Test files live in `tests/`. All tests require a live server on ws://localhost:9001.

Run commands (from project root):
```
source .venv/bin/activate
python tests/run_tests.py           # all suites
python tests/test_game_states.py    # individual suite
```

Existing files:
- `tests/test_multiplayer.py` — original, monolithic test (connect/welcome/2-player/reject/disconnect)
- `tests/helpers.py` — shared constants and async helpers (connect_player, drain_until, start_game, etc.)
- `tests/test_game_states.py` — 12 tests: menu/playing/paused/gameover/restart/quit transitions
- `tests/test_movement.py` — 13 tests: spawn positions, invuln, WASD movement, speed, border collision, pause
- `tests/test_shooting.py` — 10 tests: bullet spawn, direction, owner, cooldown, wall destruction, particles, pause
- `tests/test_waves.py` — 12 tests: wave init, enemy spawn, colors, HP, cap, score, wave advance, gameover, restart
- `tests/test_edge_cases.py` — 16 tests: malformed JSON, large payloads, unknown types, rapid connect/disconnect, state schema, wall invariants

**Why:** Tests require a running server — there is no mock/unit-test setup for the C++ engine.
