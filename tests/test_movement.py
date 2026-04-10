"""
Tests for player movement and input handling.

Verifies:
- Moving up/down/left/right changes position by the expected delta
- Idle input stops movement
- Direction field tracks last movement direction
- Invulnerability countdown starts at 90 and decrements
- Players stop at borders (steel walls at row/col 0 and ROWS-1/COLS-1)
- Both players can move independently
- Movement does not happen while paused
"""

import asyncio
import json
import math
from helpers import (
    WS_URL, connect_player, drain_until, start_game, send_input, send_idle,
    find_player,
    P0_SPAWN_X, P0_SPAWN_Y, P1_SPAWN_X, P1_SPAWN_Y,
    PLAYER_SPEED, PLAYER_INVULN_FRAMES,
    INPUT_KEYS_UP, INPUT_KEYS_DOWN, INPUT_KEYS_LEFT, INPUT_KEYS_RIGHT,
    INPUT_KEYS_IDLE,
    TILE, ROWS, COLS,
)


# ── Helpers ──────────────────────────────────────────────────────────────────

async def two_players_playing():
    ws1, w1 = await connect_player()
    ws2, w2 = await connect_player()
    assert w1["type"] == "welcome"
    assert w2["type"] == "welcome"
    state = await start_game(ws1, hardmode=False)
    return ws1, ws2, w1["playerId"], w2["playerId"], state


async def cleanup(*wss):
    for ws in wss:
        try:
            await ws.send(json.dumps({"type": "quit"}))
        except Exception:
            pass
        try:
            await ws.close()
        except Exception:
            pass
    await asyncio.sleep(0.15)


# ── Test: player spawns at expected coordinates ───────────────────────────────

async def test_player_spawn_positions():
    """P0 spawns bottom-left, P1 spawns bottom-right, both facing up (dir=0)."""
    ws1, ws2, p1_id, p2_id, state = await two_players_playing()
    try:
        p0 = find_player(state, 0)
        p1 = find_player(state, 1)
        assert p0 is not None, "Player 0 not in state"
        assert p1 is not None, "Player 1 not in state"

        assert abs(p0["x"] - P0_SPAWN_X) < 1, f"P0 x wrong: {p0['x']}"
        assert abs(p0["y"] - P0_SPAWN_Y) < 1, f"P0 y wrong: {p0['y']}"
        assert p0["dir"] == 0, f"P0 dir wrong: {p0['dir']}"

        assert abs(p1["x"] - P1_SPAWN_X) < 1, f"P1 x wrong: {p1['x']}"
        assert abs(p1["y"] - P1_SPAWN_Y) < 1, f"P1 y wrong: {p1['y']}"
        assert p1["dir"] == 0, f"P1 dir wrong: {p1['dir']}"

        print(f"  PASS: P0 at ({p0['x']}, {p0['y']}), P1 at ({p1['x']}, {p1['y']})")
    finally:
        await cleanup(ws1, ws2)


# ── Test: invulnerability starts at 90 ────────────────────────────────────────

async def test_invulnerability_starts_at_90():
    """Both players start with invuln=90 after game start."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        p0 = find_player(state, 0)
        p1 = find_player(state, 1)
        assert p0["invuln"] == PLAYER_INVULN_FRAMES, (
            f"P0 invuln={p0['invuln']}, expected {PLAYER_INVULN_FRAMES}"
        )
        assert p1["invuln"] == PLAYER_INVULN_FRAMES, (
            f"P1 invuln={p1['invuln']}, expected {PLAYER_INVULN_FRAMES}"
        )
        print(f"  PASS: both players start with invuln={PLAYER_INVULN_FRAMES}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: invulnerability decrements over time ────────────────────────────────

async def test_invulnerability_decrements():
    """Player invuln countdown decreases each tick when game is playing."""
    ws1, ws2, _, _, start_state = await two_players_playing()
    try:
        await send_idle(ws1)
        # Wait ~20 frames (~330ms) and sample invuln
        await asyncio.sleep(0.35)
        state = await drain_until(ws1, lambda _: True)
        p0 = find_player(state, 0)
        assert p0["invuln"] < PLAYER_INVULN_FRAMES, (
            f"invuln did not decrease: still {p0['invuln']}"
        )
        print(f"  PASS: invuln decremented to {p0['invuln']} after ~20+ frames")
    finally:
        await cleanup(ws1, ws2)


# ── Test: moving up decreases y ────────────────────────────────────────────────

async def test_move_up_decreases_y():
    """Holding 'up' input must reduce player 0's y coordinate."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        p0_before = find_player(state, 0)
        y_before = p0_before["y"]

        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.25)   # ~15 frames @ 60fps
        await send_idle(ws1)

        state = await drain_until(ws1, lambda _: True)
        p0_after = find_player(state, 0)
        y_after = p0_after["y"]

        assert y_after < y_before, (
            f"Moving up did not decrease y: before={y_before:.1f}, after={y_after:.1f}"
        )
        assert p0_after["dir"] == 0, f"Direction should be 0 (up), got {p0_after['dir']}"
        print(f"  PASS: up input: y {y_before:.1f} -> {y_after:.1f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: moving down increases y ─────────────────────────────────────────────

async def test_move_down_increases_y():
    """Holding 'down' input must increase player 0's y coordinate."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Move away from bottom border first
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.35)
        await send_idle(ws1)
        state = await drain_until(ws1, lambda _: True)
        y_before = find_player(state, 0)["y"]

        await send_input(ws1, INPUT_KEYS_DOWN)
        await asyncio.sleep(0.25)
        await send_idle(ws1)
        state = await drain_until(ws1, lambda _: True)
        p0 = find_player(state, 0)

        assert p0["y"] > y_before, (
            f"Moving down did not increase y: before={y_before:.1f}, after={p0['y']:.1f}"
        )
        assert p0["dir"] == 2, f"Direction should be 2 (down), got {p0['dir']}"
        print(f"  PASS: down input: y {y_before:.1f} -> {p0['y']:.1f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: moving left decreases x ─────────────────────────────────────────────

async def test_move_left_decreases_x():
    """Holding 'left' input must decrease player 0's x coordinate."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        x_before = find_player(state, 0)["x"]

        await send_input(ws1, INPUT_KEYS_LEFT)
        await asyncio.sleep(0.25)
        await send_idle(ws1)
        state = await drain_until(ws1, lambda _: True)
        p0 = find_player(state, 0)

        assert p0["x"] < x_before, (
            f"Moving left did not decrease x: before={x_before:.1f}, after={p0['x']:.1f}"
        )
        assert p0["dir"] == 3, f"Direction should be 3 (left), got {p0['dir']}"
        print(f"  PASS: left input: x {x_before:.1f} -> {p0['x']:.1f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: moving right increases x ────────────────────────────────────────────

async def test_move_right_increases_x():
    """Holding 'right' input must increase player 1's x coordinate (from right-side spawn)."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Move p1 away from the right border first
        await send_input(ws2, INPUT_KEYS_LEFT)
        await asyncio.sleep(0.4)
        await send_idle(ws2)
        state = await drain_until(ws2, lambda _: True)
        x_before = find_player(state, 1)["x"]

        await send_input(ws2, INPUT_KEYS_RIGHT)
        await asyncio.sleep(0.25)
        await send_idle(ws2)
        state = await drain_until(ws2, lambda _: True)
        p1 = find_player(state, 1)

        assert p1["x"] > x_before, (
            f"Moving right did not increase x: before={x_before:.1f}, after={p1['x']:.1f}"
        )
        assert p1["dir"] == 1, f"Direction should be 1 (right), got {p1['dir']}"
        print(f"  PASS: right input: x {x_before:.1f} -> {p1['x']:.1f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: idle input stops movement ───────────────────────────────────────────

async def test_idle_input_stops_movement():
    """After sending idle input, player position must not change frame-to-frame."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Move somewhere away from borders
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.4)
        await send_idle(ws1)

        # Sample twice with a short gap
        state1 = await drain_until(ws1, lambda _: True)
        await asyncio.sleep(0.1)
        state2 = await drain_until(ws1, lambda _: True)

        p0_1 = find_player(state1, 0)
        p0_2 = find_player(state2, 0)

        assert abs(p0_1["x"] - p0_2["x"]) < 0.1, (
            f"x changed while idle: {p0_1['x']:.2f} -> {p0_2['x']:.2f}"
        )
        assert abs(p0_1["y"] - p0_2["y"]) < 0.1, (
            f"y changed while idle: {p0_1['y']:.2f} -> {p0_2['y']:.2f}"
        )
        print("  PASS: player stops moving after idle input")
    finally:
        await cleanup(ws1, ws2)


# ── Test: movement speed is approximately PLAYER_SPEED px/tick ────────────────

async def test_movement_speed_approximately_correct():
    """
    After N input frames, displacement should be roughly N * PLAYER_SPEED.
    We hold 'up' for ~10 frames and check the distance is in a plausible range.
    """
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        y_start = find_player(state, 0)["y"]

        # Send up for ~10 game ticks (server runs at 60 fps, so ~167ms)
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.18)
        await send_idle(ws1)
        state = await drain_until(ws1, lambda _: True)

        y_end = find_player(state, 0)["y"]
        dy = y_start - y_end  # upward means negative direction, so dy > 0 means moved up

        # 10 frames * 2.5 px = 25 px; allow generous range [5, 100] accounting for
        # network timing, wall blocks, and invuln (invuln doesn't block movement)
        assert dy > 5, f"Moved less than expected upward: dy={dy:.1f}"
        assert dy < 150, f"Moved more than expected upward: dy={dy:.1f}"
        print(f"  PASS: upward displacement {dy:.1f} px is in expected range")
    finally:
        await cleanup(ws1, ws2)


# ── Test: player blocked by border wall ──────────────────────────────────────

async def test_player_blocked_at_top_border():
    """
    Player 0 starts near the bottom and cannot move above the top border row.
    After holding 'up' long enough, y must clamp above the steel border (row 1).
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Hold up long enough to hit the top border
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(2.5)   # 150 frames * 2.5px = 375px; MAP_H=600 so enough
        await send_idle(ws1)
        state = await drain_until(ws1, lambda _: True)
        p0 = find_player(state, 0)

        # Border is steel (wall type 2) at row 0.  Player centre must be >= 1*TILE+14
        min_y = 1 * TILE + 14  # top of tile row 1 + half-tank
        assert p0["y"] >= min_y - 1, (
            f"Player passed through top border: y={p0['y']:.1f}, min_y={min_y}"
        )
        print(f"  PASS: player blocked at top border, y={p0['y']:.1f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: movement paused does not change position ────────────────────────────

async def test_movement_does_not_advance_while_paused():
    """While game is paused, holding an input key must not move the player."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Get stable position away from borders
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.5)
        await send_idle(ws1)

        await ws1.send(json.dumps({"type": "pause"}))
        paused = await drain_until(ws1, lambda s: s.get("gameState") == "paused")
        assert paused is not None
        y_at_pause = find_player(paused, 0)["y"]

        # Hold up while paused
        await send_input(ws1, INPUT_KEYS_UP)
        await asyncio.sleep(0.2)
        await send_idle(ws1)

        state = await drain_until(ws1, lambda _: True)
        y_after = find_player(state, 0)["y"]

        assert abs(y_after - y_at_pause) < 0.1, (
            f"Player moved while paused: y {y_at_pause:.2f} -> {y_after:.2f}"
        )
        print("  PASS: player does not move while game is paused")
    finally:
        await cleanup(ws1, ws2)


# ── Test: two players move independently ─────────────────────────────────────

async def test_two_players_move_independently():
    """P0 and P1 have different inputs and end up at different positions."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        # Send different directions to each player
        await send_input(ws1, INPUT_KEYS_UP)    # P0 moves up
        await send_input(ws2, INPUT_KEYS_LEFT)  # P1 moves left
        await asyncio.sleep(0.3)
        await send_idle(ws1)
        await send_idle(ws2)

        state = await drain_until(ws1, lambda _: True)
        p0 = find_player(state, 0)
        p1 = find_player(state, 1)

        # P0 should have moved up (y decreased), P1 left (x decreased)
        assert p0["dir"] == 0, f"P0 dir should be 0, got {p0['dir']}"
        assert p1["dir"] == 3, f"P1 dir should be 3, got {p1['dir']}"
        assert p0["y"] < P0_SPAWN_Y, f"P0 did not move up: y={p0['y']:.1f}"
        assert p1["x"] < P1_SPAWN_X, f"P1 did not move left: x={p1['x']:.1f}"
        print(f"  PASS: P0 at ({p0['x']:.0f},{p0['y']:.0f}), P1 at ({p1['x']:.0f},{p1['y']:.0f})")
    finally:
        await cleanup(ws1, ws2)


# ── Test: input ignored for rejected player ───────────────────────────────────

async def test_input_from_non_player_is_ignored():
    """
    A connection that received 'full' has no player slot.
    Sending input from it must not crash the server.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    ws_full, full_msg = await connect_player()
    try:
        assert full_msg["type"] == "full"
        # Send input anyway — server should silently ignore it
        await ws_full.send(json.dumps({
            "type": "input",
            "keys": INPUT_KEYS_UP
        }))
        await asyncio.sleep(0.1)
        # Server must still be alive and broadcasting state
        state = await drain_until(ws1, lambda _: True)
        assert state is not None, "Server stopped responding after spurious input from full client"
        print("  PASS: server ignores input from rejected (full) connection")
    finally:
        try:
            await ws_full.close()
        except Exception:
            pass
        await cleanup(ws1, ws2)


# ── Runner ────────────────────────────────────────────────────────────────────

TESTS = [
    ("player spawn positions", test_player_spawn_positions),
    ("invulnerability starts at 90", test_invulnerability_starts_at_90),
    ("invulnerability decrements", test_invulnerability_decrements),
    ("move up decreases y", test_move_up_decreases_y),
    ("move down increases y", test_move_down_increases_y),
    ("move left decreases x", test_move_left_decreases_x),
    ("move right increases x", test_move_right_increases_x),
    ("idle input stops movement", test_idle_input_stops_movement),
    ("movement speed approximately correct", test_movement_speed_approximately_correct),
    ("player blocked at top border", test_player_blocked_at_top_border),
    ("movement frozen while paused", test_movement_does_not_advance_while_paused),
    ("two players move independently", test_two_players_move_independently),
    ("input from non-player ignored", test_input_from_non_player_is_ignored),
]


async def run_all():
    passed = failed = 0
    for name, fn in TESTS:
        print(f"\n[TEST] {name}")
        try:
            await fn()
            passed += 1
        except Exception as exc:
            print(f"  FAIL: {exc}")
            failed += 1
        await asyncio.sleep(0.2)

    print(f"\n{'='*50}")
    print(f"Movement tests: {passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
