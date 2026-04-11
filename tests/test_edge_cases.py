"""
Edge-case and robustness tests.

Covers:
- Malformed JSON is silently ignored (server keeps running)
- Empty message is silently ignored
- Unknown message type is silently ignored
- Message with missing 'keys' sub-object in 'input' is silently ignored
- Rapid connect/disconnect does not crash or corrupt server state
- Third player gets 'full' while game is in playing state
- Sending start while game is already playing restarts it
- Sending start with missing hardmode key defaults to false
- Very large/garbage message does not crash server
- Concurrent inputs from both players in the same frame are both processed
- State shape invariants: required fields are always present
"""

import asyncio
import json
import random
import string
from helpers import (
    WS_URL, connect_player, drain_until, start_game, send_input, send_idle,
    find_player,
    INPUT_KEYS_UP, INPUT_KEYS_IDLE,
    P0_SPAWN_Y, TILE,
)


# ── Helpers ──────────────────────────────────────────────────────────────────

async def two_players_playing(hardmode=False):
    ws1, w1 = await connect_player()
    ws2, w2 = await connect_player()
    assert w1["type"] == "welcome"
    assert w2["type"] == "welcome"
    state = await start_game(ws1, hardmode=hardmode)
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
    await asyncio.sleep(0.2)


async def server_still_alive(ws):
    """Return True if we can still receive a state update from the server."""
    state = await drain_until(ws, lambda _: True, max_msgs=10, timeout_per_msg=0.5)
    return state is not None


# ── Test: malformed JSON is ignored ──────────────────────────────────────────

async def test_malformed_json_ignored():
    """Server must not crash when receiving invalid JSON."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await ws1.send("{not valid json!!!")
        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), "Server stopped responding after malformed JSON"
        print("  PASS: server survived malformed JSON")
    finally:
        await cleanup(ws1, ws2)


# ── Test: empty message is ignored ───────────────────────────────────────────

async def test_empty_message_ignored():
    """Server must not crash when receiving an empty string."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await ws1.send("")
        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), "Server stopped responding after empty message"
        print("  PASS: server survived empty message")
    finally:
        await cleanup(ws1, ws2)


# ── Test: unknown message type is ignored ─────────────────────────────────────

async def test_unknown_type_ignored():
    """Server must silently ignore a message with an unrecognised 'type'."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await ws1.send(json.dumps({"type": "teleport", "x": 0, "y": 0}))
        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), "Server stopped after unknown message type"
        print("  PASS: server ignored unknown message type")
    finally:
        await cleanup(ws1, ws2)


# ── Test: input with missing 'keys' is ignored ───────────────────────────────

async def test_input_without_keys_ignored():
    """An 'input' message missing the 'keys' object must be silently ignored."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await ws1.send(json.dumps({"type": "input"}))  # no 'keys' field
        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), "Server stopped after input without keys"
        print("  PASS: server ignored input missing 'keys'")
    finally:
        await cleanup(ws1, ws2)


# ── Test: very large garbage message is ignored ───────────────────────────────

async def test_large_garbage_message_ignored():
    """A very large random payload must not crash the server."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        garbage = ''.join(random.choices(string.printable, k=65536))
        await ws1.send(garbage)
        await asyncio.sleep(0.15)
        assert await server_still_alive(ws1), "Server stopped after large garbage message"
        print("  PASS: server survived 64KB garbage message")
    finally:
        await cleanup(ws1, ws2)


# ── Test: partially-formed JSON keys are ignored ──────────────────────────────

async def test_partial_json_keys_ignored():
    """JSON with correct outer shape but wrong value types must not crash."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # 'hardmode' expects bool but gets a string; 'keys' expects object but gets array
        await ws1.send('{"type":"start","hardmode":"yes"}')
        await ws1.send('{"type":"input","keys":[1,2,3]}')
        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), "Server stopped after bad value types"
        print("  PASS: server survived messages with wrong value types")
    finally:
        await cleanup(ws1, ws2)


# ── Test: rapid connect/disconnect does not corrupt state ─────────────────────

async def test_rapid_connect_disconnect():
    """
    Open and close 5 connections in rapid succession without joining a game.
    After all disconnect, original P0 and P1 connections should still work.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Rapid open/close of additional connections
        # (they will get 'full' responses since 2 players are active)
        for _ in range(5):
            ws_tmp, msg = await connect_player()
            # Should be rejected or welcomed depending on timing
            await asyncio.sleep(0.02)
            await ws_tmp.close()

        await asyncio.sleep(0.1)
        assert await server_still_alive(ws1), (
            "Server state corrupted after rapid connect/disconnect"
        )
        print("  PASS: server stable after rapid connect/disconnect")
    finally:
        await cleanup(ws1, ws2)


# ── Test: third player rejected while game is playing ────────────────────────

async def test_third_player_rejected_during_playing():
    """When 2 players are active and game is playing, a 3rd connection gets 'full'."""
    ws1, ws2, _, _, _ = await two_players_playing()
    ws3 = None
    try:
        ws3, msg = await connect_player()
        assert msg["type"] == "full", (
            f"Expected 'full' for 3rd player, got: {msg}"
        )
        print("  PASS: 3rd player correctly rejected with 'full' message")
    finally:
        if ws3:
            try:
                await ws3.close()
            except Exception:
                pass
        await cleanup(ws1, ws2)


# ── Test: start while playing restarts the game ───────────────────────────────

async def test_start_while_playing_restarts():
    """
    Sending 'start' when the game is already in 'playing' state should restart
    (same as calling restart): score resets to 0, wave resets to 1.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Wait a moment then restart via 'start'
        await asyncio.sleep(0.1)
        await ws1.send(json.dumps({"type": "start", "hardmode": False}))
        state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert state is not None
        assert state["score"] == 0, f"Score not reset: {state['score']}"
        assert state["wave"] == 1, f"Wave not reset: {state['wave']}"
        print("  PASS: sending start while playing restarts the game")
    finally:
        await cleanup(ws1, ws2)


# ── Test: start with no hardmode key defaults to normal ───────────────────────

async def test_start_missing_hardmode_defaults_to_false():
    """
    The server's jsonGetBool returns false when the key is absent.
    So starting without 'hardmode' key must use hardmode=false.
    """
    ws1, ws2, _, p2_id, _ = await two_players_playing()
    try:
        # Restart with a start message that has no hardmode field
        await ws1.send('{"type":"start"}')
        state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert state is not None
        assert state["hardmode"] is False, (
            f"Expected hardmode=false when key absent, got {state['hardmode']}"
        )
        p1 = find_player(state, p2_id)
        if p1:
            assert p1["lives"] == 3, (
                f"Expected 3 lives in normal mode, got {p1['lives']}"
            )
        print("  PASS: missing hardmode key defaults to false (3 lives)")
    finally:
        await cleanup(ws1, ws2)


# ── Test: concurrent inputs from both players processed in same tick ──────────

async def test_concurrent_inputs_both_processed():
    """
    Sending input simultaneously from both players must result in both
    players moving — neither input should be lost.
    """
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        y0_before = find_player(state, 0)["y"]
        x1_before = find_player(state, 1)["x"]

        # Send both at the same time
        await asyncio.gather(
            send_input(ws1, INPUT_KEYS_UP),         # P0 moves up
            send_input(ws2, {"up": False, "down": False, "left": True,
                              "right": False, "shoot": False}),  # P1 moves left
        )
        await asyncio.sleep(0.3)
        await asyncio.gather(send_idle(ws1), send_idle(ws2))

        state = await drain_until(ws1, lambda _: True)
        y0_after = find_player(state, 0)["y"]
        x1_after = find_player(state, 1)["x"]

        assert y0_after < y0_before, (
            f"P0 did not move up: before={y0_before:.1f}, after={y0_after:.1f}"
        )
        assert x1_after < x1_before, (
            f"P1 did not move left: before={x1_before:.1f}, after={x1_after:.1f}"
        )
        print(f"  PASS: both inputs processed, P0 y {y0_before:.0f}->{y0_after:.0f}, "
              f"P1 x {x1_before:.0f}->{x1_after:.0f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: state message schema invariants ─────────────────────────────────────

async def test_state_schema_invariants():
    """
    Every state message must contain the required top-level fields.
    No top-level 'lives' field should exist (it moved to per-player).
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Sample a few state messages and check schema
        required_fields = {
            "type", "gameState", "score", "wave", "enemiesLeft",
            "screenShake", "frameCount", "hardmode", "waveClearTimer",
            "players", "walls", "enemies", "bullets", "particles"
        }
        samples_checked = 0
        for _ in range(20):
            state = await drain_until(ws1, lambda _: True, max_msgs=5, timeout_per_msg=0.1)
            if state is None:
                break
            missing = required_fields - set(state.keys())
            assert not missing, f"State missing required fields: {missing}"
            assert "lives" not in state, "Top-level 'lives' must not exist in state"
            samples_checked += 1

        assert samples_checked > 0, "Did not receive any state messages to check"
        print(f"  PASS: {samples_checked} state messages all have correct schema")
    finally:
        await cleanup(ws1, ws2)


# ── Test: walls field is 15 rows x 20 cols ────────────────────────────────────

async def test_walls_dimensions():
    """The walls array must always be 15 rows of 20 columns."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        walls = state["walls"]
        assert len(walls) == 15, f"Expected 15 rows, got {len(walls)}"
        for row_idx, row in enumerate(walls):
            assert len(row) == 20, (
                f"Row {row_idx} has {len(row)} cols, expected 20"
            )
        print("  PASS: walls array is 15x20")
    finally:
        await cleanup(ws1, ws2)


# ── Test: border walls are always steel ───────────────────────────────────────

async def test_border_walls_always_steel():
    """
    Rows 0 and 14, and columns 0 and 19, must always be steel (value=2).
    This is enforced by generateWalls() every time the map regenerates.
    """
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        walls = state["walls"]
        ROWS, COLS = 15, 20

        # Top and bottom rows
        for x in range(COLS):
            assert walls[0][x] == 2, f"walls[0][{x}]={walls[0][x]}, expected 2"
            assert walls[ROWS-1][x] == 2, f"walls[{ROWS-1}][{x}]={walls[ROWS-1][x]}, expected 2"
        # Left and right columns
        for y in range(ROWS):
            assert walls[y][0] == 2, f"walls[{y}][0]={walls[y][0]}, expected 2"
            assert walls[y][COLS-1] == 2, f"walls[{y}][{COLS-1}]={walls[y][COLS-1]}, expected 2"

        print("  PASS: border walls are all steel (value=2)")
    finally:
        await cleanup(ws1, ws2)


# ── Test: player spawn areas are always clear ─────────────────────────────────

async def test_player_spawn_areas_clear():
    """
    generateWalls() explicitly protects player spawn areas.
    P0 spawns near col 1-2, rows 11-14.
    P1 spawns near col 17-18, rows 11-14.
    These should never have walls placed on them.
    """
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        walls = state["walls"]
        # P0 spawn: x < 3 && y > ROWS - 5 = y > 10, i.e. rows 11-13, cols 0-2
        for row in range(11, 14):
            for col in range(1, 3):    # col 0 is border (always steel, expected)
                assert walls[row][col] in (0, 2) or col == 0, (
                    f"Unexpected interior wall at spawn area P0: walls[{row}][{col}]={walls[row][col]}"
                )
        # P1 spawn: x > COLS - 4 = x > 16, i.e. cols 17-18, rows 11-13
        for row in range(11, 14):
            for col in range(17, 19):  # col 19 is border
                assert walls[row][col] in (0, 2) or col == 19, (
                    f"Unexpected interior wall at spawn area P1: walls[{row}][{col}]={walls[row][col]}"
                )
        print("  PASS: player spawn areas are free of interior walls")
    finally:
        await cleanup(ws1, ws2)


# ── Test: frameCount increases monotonically ─────────────────────────────────

async def test_frame_count_increases_monotonically():
    """frameCount must never decrease between consecutive state messages."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        prev_frame = -1
        for _ in range(30):
            state = await drain_until(ws1, lambda _: True, max_msgs=3, timeout_per_msg=0.1)
            if state is None:
                break
            fc = state.get("frameCount", 0)
            assert fc >= prev_frame, (
                f"frameCount decreased: {prev_frame} -> {fc}"
            )
            prev_frame = fc
        print(f"  PASS: frameCount monotonically increased to {prev_frame}")
    finally:
        await cleanup(ws1, ws2)


# ── Runner ────────────────────────────────────────────────────────────────────

TESTS = [
    ("malformed JSON ignored", test_malformed_json_ignored),
    ("empty message ignored", test_empty_message_ignored),
    ("unknown type ignored", test_unknown_type_ignored),
    ("input without keys ignored", test_input_without_keys_ignored),
    ("large garbage message ignored", test_large_garbage_message_ignored),
    ("partial JSON keys ignored", test_partial_json_keys_ignored),
    ("rapid connect/disconnect stability", test_rapid_connect_disconnect),
    ("third player rejected during playing", test_third_player_rejected_during_playing),
    ("start while playing restarts", test_start_while_playing_restarts),
    ("missing hardmode defaults to false", test_start_missing_hardmode_defaults_to_false),
    ("concurrent inputs both processed", test_concurrent_inputs_both_processed),
    ("state schema invariants", test_state_schema_invariants),
    ("walls dimensions 15x20", test_walls_dimensions),
    ("border walls always steel", test_border_walls_always_steel),
    ("player spawn areas clear", test_player_spawn_areas_clear),
    ("frameCount increases monotonically", test_frame_count_increases_monotonically),
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
    print(f"Edge-case tests: {passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
