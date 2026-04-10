"""Shared helpers for all tank-game test modules."""

import asyncio
import json
import websockets

WS_URL = "ws://localhost:9001"

# Constants mirrored from game_engine.h / game_engine.cpp
TILE = 40
COLS = 20
ROWS = 15
MAP_W = 800
MAP_H = 600

# Player spawn positions (centre of tank)
P0_SPAWN_X = 1.5 * TILE     # 60.0
P0_SPAWN_Y = (ROWS - 2.5) * TILE  # 500.0
P1_SPAWN_X = (COLS - 2.5) * TILE  # 700.0
P1_SPAWN_Y = (ROWS - 2.5) * TILE  # 500.0

PLAYER_SPEED = 2.5          # pixels per tick
PLAYER_COOLDOWN = 15        # ticks between shots
PLAYER_INVULN_FRAMES = 90   # frames of spawn invulnerability
WAVE1_ENEMIES = 4           # enemiesLeft at start (from game_engine.cpp: enemiesLeft = 4)

INPUT_KEYS_IDLE = {"up": False, "down": False, "left": False, "right": False, "shoot": False}
INPUT_KEYS_UP   = {"up": True,  "down": False, "left": False, "right": False, "shoot": False}
INPUT_KEYS_DOWN = {"up": False, "down": True,  "left": False, "right": False, "shoot": False}
INPUT_KEYS_LEFT = {"up": False, "down": False, "left": True,  "right": False, "shoot": False}
INPUT_KEYS_RIGHT= {"up": False, "down": False, "left": False, "right": True,  "shoot": False}
INPUT_KEYS_SHOOT= {"up": False, "down": False, "left": False, "right": False, "shoot": True}


async def connect_player():
    """Connect a single player and return (ws, player_id)."""
    ws = await websockets.connect(WS_URL)
    msg = await asyncio.wait_for(ws.recv(), timeout=3)
    data = json.loads(msg)
    return ws, data


async def drain_until(ws, condition, max_msgs=120, timeout_per_msg=2):
    """
    Read messages until condition(state_dict) returns True.
    Only considers messages of type 'state'.
    Returns the matching state, or None if max_msgs exhausted.
    """
    for _ in range(max_msgs):
        try:
            raw = await asyncio.wait_for(ws.recv(), timeout=timeout_per_msg)
        except asyncio.TimeoutError:
            return None
        state = json.loads(raw)
        if state.get("type") == "state" and condition(state):
            return state
    return None


async def drain_n(ws, n, timeout_per_msg=2):
    """Consume exactly n messages and return the last one parsed."""
    last = None
    for _ in range(n):
        raw = await asyncio.wait_for(ws.recv(), timeout=timeout_per_msg)
        last = json.loads(raw)
    return last


async def get_current_state(ws, max_msgs=10):
    """Return the next state message (skip non-state messages)."""
    return await drain_until(ws, lambda _: True, max_msgs=max_msgs)


async def start_game(ws1, hardmode=False):
    """Send start and wait until gameState == 'playing'."""
    await ws1.send(json.dumps({"type": "start", "hardmode": hardmode}))
    state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
    assert state is not None, "Game never reached 'playing' state after start"
    return state


async def send_input(ws, keys):
    """Send an input message."""
    await ws.send(json.dumps({"type": "input", "keys": keys}))


async def send_idle(ws):
    """Send an all-false input (stop all movement and shooting)."""
    await send_input(ws, INPUT_KEYS_IDLE)


def find_player(state, player_id):
    """Return the player dict with the given id, or None."""
    for p in state.get("players", []):
        if p["id"] == player_id:
            return p
    return None
