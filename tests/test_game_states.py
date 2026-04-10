"""
Tests for game state transitions.

Covers: menu -> playing -> paused -> resume -> gameover -> restart -> quit -> menu
Each test starts a fresh game so they can run independently (server state resets via quit/restart).
"""

import asyncio
import json
import websockets
from helpers import (
    WS_URL, connect_player, drain_until, start_game,
    send_idle, find_player, WAVE1_ENEMIES,
)


# ── helpers ─────────────────────────────────────────────────────────────────

async def two_players():
    """Connect two players. Returns (ws1, ws2, p1_id, p2_id)."""
    ws1, w1 = await connect_player()
    assert w1["type"] == "welcome"
    p1_id = w1["playerId"]

    ws2, w2 = await connect_player()
    assert w2["type"] == "welcome"
    p2_id = w2["playerId"]

    return ws1, ws2, p1_id, p2_id


async def cleanup(*websockets_list):
    """Close all websockets, suppressing errors."""
    for ws in websockets_list:
        try:
            await ws.close()
        except Exception:
            pass
    # Give server a moment to process disconnects before next test
    await asyncio.sleep(0.15)


# ── Test: initial state is menu ──────────────────────────────────────────────

async def test_initial_state_is_menu():
    """Server broadcasts 'menu' gameState before any game has started."""
    ws1, _ = await connect_player()
    try:
        state = await drain_until(ws1, lambda _: True)
        assert state is not None, "Did not receive any state message"
        assert state["type"] == "state"
        assert state["gameState"] == "menu", (
            f"Expected 'menu' but got '{state['gameState']}'"
        )
        print("  PASS: initial state is menu")
    finally:
        await cleanup(ws1)


# ── Test: start transitions to playing ──────────────────────────────────────

async def test_start_transitions_to_playing():
    """Sending 'start' moves gameState from menu to playing."""
    ws1, ws2, _, _ = await two_players()
    try:
        state = await start_game(ws1, hardmode=False)
        assert state["gameState"] == "playing"
        assert state["score"] == 0
        assert state["wave"] == 1
        assert state["enemiesLeft"] == WAVE1_ENEMIES
        assert state["hardmode"] is False
        print("  PASS: start transitions to playing, score=0, wave=1, enemiesLeft=4")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: hardmode flag is carried into state ────────────────────────────────

async def test_start_hardmode_flag():
    """Starting with hardmode=True sets state.hardmode=true and reduces lives to 1."""
    ws1, ws2, p1_id, _ = await two_players()
    try:
        state = await start_game(ws1, hardmode=True)
        assert state["hardmode"] is True, "hardmode flag not set in state"
        p1 = find_player(state, p1_id)
        assert p1 is not None
        assert p1["lives"] == 1, (
            f"Expected 1 life in hardmode, got {p1['lives']}"
        )
        print("  PASS: hardmode flag propagated, player lives=1")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: pause transitions correctly ────────────────────────────────────────

async def test_pause_and_resume():
    """PLAYING -> pause -> PAUSED -> resume -> PLAYING."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)

        # Pause
        await ws1.send(json.dumps({"type": "pause"}))
        paused = await drain_until(ws1, lambda s: s.get("gameState") == "paused")
        assert paused is not None, "State never became 'paused'"
        print("  PASS: game paused successfully")

        # Resume
        await ws1.send(json.dumps({"type": "resume"}))
        playing = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert playing is not None, "State never returned to 'playing' after resume"
        print("  PASS: game resumed successfully")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: pause is idempotent ─────────────────────────────────────────────────

async def test_double_pause_stays_paused():
    """Sending 'pause' when already paused keeps gameState paused."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        await ws1.send(json.dumps({"type": "pause"}))
        await drain_until(ws1, lambda s: s.get("gameState") == "paused")

        # Second pause — should have no effect
        await ws1.send(json.dumps({"type": "pause"}))
        await asyncio.sleep(0.1)
        state = await drain_until(ws1, lambda _: True)
        assert state["gameState"] == "paused", (
            f"Expected 'paused' after double-pause, got '{state['gameState']}'"
        )
        print("  PASS: double-pause keeps state paused")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: resume without pause is a no-op ─────────────────────────────────────

async def test_resume_without_pause_stays_playing():
    """Sending 'resume' while already playing keeps gameState playing."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        await ws1.send(json.dumps({"type": "resume"}))
        await asyncio.sleep(0.1)
        state = await drain_until(ws1, lambda _: True)
        assert state["gameState"] == "playing", (
            f"Expected 'playing' after spurious resume, got '{state['gameState']}'"
        )
        print("  PASS: spurious resume does not change state")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: ticks do not advance while paused ───────────────────────────────────

async def test_frame_count_frozen_while_paused():
    """frameCount must not increase while the game is paused."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        await ws1.send(json.dumps({"type": "pause"}))
        paused = await drain_until(ws1, lambda s: s.get("gameState") == "paused")
        assert paused is not None

        frame_at_pause = paused["frameCount"]
        # Wait a few real-time frames
        await asyncio.sleep(0.15)
        state = await drain_until(ws1, lambda _: True)
        frame_after_wait = state["frameCount"]

        assert frame_after_wait == frame_at_pause, (
            f"frameCount advanced while paused: {frame_at_pause} -> {frame_after_wait}"
        )
        print(f"  PASS: frameCount frozen at {frame_at_pause} while paused")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: quit returns to menu ────────────────────────────────────────────────

async def test_quit_returns_to_menu():
    """Sending 'quit' from playing state returns to menu."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        await ws1.send(json.dumps({"type": "quit"}))
        menu = await drain_until(ws1, lambda s: s.get("gameState") == "menu")
        assert menu is not None, "State never returned to menu after quit"
        print("  PASS: quit returns to menu")
    finally:
        await cleanup(ws1, ws2)


# ── Test: quit from paused returns to menu ────────────────────────────────────

async def test_quit_from_paused_returns_to_menu():
    """Sending 'quit' while paused should also return to menu."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        await ws1.send(json.dumps({"type": "pause"}))
        await drain_until(ws1, lambda s: s.get("gameState") == "paused")
        await ws1.send(json.dumps({"type": "quit"}))
        menu = await drain_until(ws1, lambda s: s.get("gameState") == "menu")
        assert menu is not None, "State never returned to menu after quit-from-pause"
        print("  PASS: quit from paused returns to menu")
    finally:
        await cleanup(ws1, ws2)


# ── Test: restart resets score and wave ───────────────────────────────────────

async def test_restart_resets_game():
    """Sending 'restart' resets score=0, wave=1, enemiesLeft=4."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1)
        # Let a few frames pass to accumulate frameCount
        await asyncio.sleep(0.15)

        await ws1.send(json.dumps({"type": "restart"}))
        state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert state is not None, "Game never reached playing after restart"
        assert state["score"] == 0, f"Score not reset: {state['score']}"
        assert state["wave"] == 1, f"Wave not reset: {state['wave']}"
        assert state["enemiesLeft"] == WAVE1_ENEMIES, (
            f"enemiesLeft not reset: {state['enemiesLeft']}"
        )
        print("  PASS: restart resets score, wave, and enemiesLeft")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: restart preserves hardmode ─────────────────────────────────────────

async def test_restart_preserves_hardmode():
    """After restart, hardmode flag matches the flag from the original start."""
    ws1, ws2, _, _ = await two_players()
    try:
        await start_game(ws1, hardmode=True)
        await ws1.send(json.dumps({"type": "restart"}))
        state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert state is not None
        assert state["hardmode"] is True, "hardmode lost after restart"
        print("  PASS: hardmode preserved across restart")
    finally:
        await ws1.send(json.dumps({"type": "quit"}))
        await cleanup(ws1, ws2)


# ── Test: last player disconnect returns to menu ──────────────────────────────

async def test_last_player_disconnect_returns_to_menu():
    """When the last active player disconnects during playing, state -> menu."""
    ws1, p1_data = await connect_player()
    assert p1_data["type"] == "welcome"
    try:
        state = await start_game(ws1)
        assert state["gameState"] == "playing"

        # Disconnect the only player
        await ws1.close()
        # Reconnect to verify state is now menu
        ws_observer, _ = await connect_player()
        try:
            menu = await drain_until(ws_observer, lambda s: s.get("gameState") == "menu")
            assert menu is not None, "State did not return to menu after last player disconnect"
            print("  PASS: last player disconnect returns game to menu")
        finally:
            await cleanup(ws_observer)
    except Exception:
        await cleanup(ws1)
        raise


# ── Runner ───────────────────────────────────────────────────────────────────

TESTS = [
    ("initial state is menu", test_initial_state_is_menu),
    ("start transitions to playing", test_start_transitions_to_playing),
    ("start hardmode flag", test_start_hardmode_flag),
    ("pause and resume", test_pause_and_resume),
    ("double pause stays paused", test_double_pause_stays_paused),
    ("resume without pause is no-op", test_resume_without_pause_stays_playing),
    ("frame count frozen while paused", test_frame_count_frozen_while_paused),
    ("quit returns to menu", test_quit_returns_to_menu),
    ("quit from paused returns to menu", test_quit_from_paused_returns_to_menu),
    ("restart resets game", test_restart_resets_game),
    ("restart preserves hardmode", test_restart_preserves_hardmode),
    ("last player disconnect returns to menu", test_last_player_disconnect_returns_to_menu),
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
        # Small gap so server fully resets between tests
        await asyncio.sleep(0.2)

    print(f"\n{'='*50}")
    print(f"Game state tests: {passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
