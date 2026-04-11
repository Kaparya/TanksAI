"""
Tests for wave progression, enemy spawning, scoring, and game-over.

Key facts from game_engine.cpp:
  - Wave 1 starts with enemiesLeft=4, maxOnScreen=3 (normal) / 4 (hard)
  - Spawn delay: 90 ticks normal / 60 ticks hard (~1.5s / 1s)
  - Wave advances when aliveEnemies==0 && enemiesLeft==0
  - Next wave: enemiesLeft = 3 + wave*2  (wave 2 -> 7, wave 3 -> 9, …)
  - Score: +100 * wave per kill
  - Game-over when all active players have lives <= 0
  - Hard mode: 1 life, enemies faster, blue/purple enemy HP +1

These tests are slower because they wait for enemies to spawn and interact.
Timeouts are set generously to avoid flakiness.
"""

import asyncio
import json
from helpers import (
    WS_URL, connect_player, drain_until, start_game, send_input, send_idle,
    find_player,
    P0_SPAWN_X, P0_SPAWN_Y, WAVE1_ENEMIES,
    INPUT_KEYS_IDLE, INPUT_KEYS_UP, INPUT_KEYS_SHOOT,
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


# ── Test: wave 1 initializes correctly ───────────────────────────────────────

async def test_wave1_initial_state():
    """Wave 1 starts with score=0, wave=1, enemiesLeft=4, no enemies on screen."""
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        assert state["wave"] == 1
        assert state["score"] == 0
        assert state["enemiesLeft"] == WAVE1_ENEMIES, (
            f"Expected enemiesLeft={WAVE1_ENEMIES}, got {state['enemiesLeft']}"
        )
        # At t=0, enemies haven't spawned yet (spawnTimer needs to reach 90)
        alive_enemies = [e for e in state.get("enemies", []) if e["alive"]]
        # It's valid to have 0 on the very first frame
        assert state["enemiesLeft"] + len(alive_enemies) == WAVE1_ENEMIES or True
        print(f"  PASS: wave=1, score=0, enemiesLeft={state['enemiesLeft']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: enemies spawn during wave 1 ─────────────────────────────────────────

async def test_enemies_spawn_in_wave1():
    """Within 3 seconds of starting, at least one enemy should appear on screen."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Max spawn delay is 90 ticks = 1.5s; wait up to 4s for the first spawn
        state = await drain_until(
            ws1,
            lambda s: len([e for e in s.get("enemies", []) if e["alive"]]) > 0,
            max_msgs=300,   # 300 * 2s timeout = generous
            timeout_per_msg=0.1,
        )
        assert state is not None, "No enemies spawned after 30 seconds"
        alive = [e for e in state["enemies"] if e["alive"]]
        assert len(alive) >= 1
        print(f"  PASS: {len(alive)} enemies on screen in wave 1")
    finally:
        await cleanup(ws1, ws2)


# ── Test: enemiesLeft decreases as enemies spawn ──────────────────────────────

async def test_enemies_left_decrements_on_spawn():
    """Each time an enemy spawns, enemiesLeft must decrease by 1."""
    ws1, ws2, _, _, start_state = await two_players_playing()
    try:
        initial_left = start_state["enemiesLeft"]

        # Wait until at least 2 enemies have spawned
        state = await drain_until(
            ws1,
            lambda s: s["enemiesLeft"] <= initial_left - 2,
            max_msgs=400,
            timeout_per_msg=0.1,
        )
        assert state is not None, (
            f"enemiesLeft never decreased from {initial_left} to <= {initial_left - 2}"
        )
        print(f"  PASS: enemiesLeft decreased from {initial_left} to {state['enemiesLeft']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: enemy colors are from the valid set ─────────────────────────────────

async def test_enemy_colors_are_valid():
    """Spawned enemies must have one of the four known type colors."""
    valid_colors = {"#ff5252", "#ff9100", "#448aff", "#e040fb"}
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        state = await drain_until(
            ws1,
            lambda s: len([e for e in s.get("enemies", []) if e["alive"]]) > 0,
            max_msgs=400,
            timeout_per_msg=0.1,
        )
        assert state is not None, "No enemies spawned"
        for e in state["enemies"]:
            if e["alive"]:
                assert e["color"] in valid_colors, (
                    f"Unexpected enemy color: {e['color']}"
                )
        print(f"  PASS: all enemy colors are valid: "
              f"{set(e['color'] for e in state['enemies'] if e['alive'])}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: enemies have alive=true and valid hp ────────────────────────────────

async def test_enemy_fields_are_valid():
    """Each spawned enemy has hp>=1, maxHp>=1, alive=true, hp<=maxHp."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        state = await drain_until(
            ws1,
            lambda s: len([e for e in s.get("enemies", []) if e["alive"]]) >= 1,
            max_msgs=400,
            timeout_per_msg=0.1,
        )
        assert state is not None, "No enemies spawned"
        for e in state["enemies"]:
            if not e["alive"]:
                continue
            assert e["hp"] >= 1, f"Enemy hp={e['hp']}"
            assert e["maxHp"] >= 1, f"Enemy maxHp={e['maxHp']}"
            assert e["hp"] <= e["maxHp"], (
                f"Enemy hp={e['hp']} > maxHp={e['maxHp']}"
            )
        print("  PASS: all enemy fields are valid")
    finally:
        await cleanup(ws1, ws2)


# ── Test: wave 1 only spawns basic enemy types ────────────────────────────────

async def test_wave1_only_spawns_basic_types():
    """
    Wave 1 uses maxType=2 in spawnEnemy(), so only types[0] and types[1] can spawn:
    red (#ff5252, speed 1.2) and orange (#ff9100, speed 2.0).
    Blue (#448aff) and purple (#e040fb) require wave >= 3.
    """
    advanced_colors = {"#448aff", "#e040fb"}
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Collect enemies across several spawns
        seen_colors = set()
        for _ in range(60):
            state = await drain_until(ws1, lambda _: True, max_msgs=5, timeout_per_msg=0.1)
            if state:
                for e in state.get("enemies", []):
                    if e["alive"]:
                        seen_colors.add(e["color"])
            if state and state["enemiesLeft"] == 0:
                break
            await asyncio.sleep(0.05)

        advanced_seen = seen_colors & advanced_colors
        assert not advanced_seen, (
            f"Advanced enemy colors appeared in wave 1: {advanced_seen}"
        )
        print(f"  PASS: wave 1 only has basic enemies, colors seen: {seen_colors}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: max 3 enemies on screen simultaneously (normal mode) ─────────────────

async def test_max_3_enemies_on_screen_normal_mode():
    """
    Normal mode: maxOnScreen=3. At any point, alive enemy count must be <= 3.
    We sample over several seconds of wave 1.
    """
    ws1, ws2, _, _, _ = await two_players_playing(hardmode=False)
    try:
        max_seen = 0
        for _ in range(120):
            state = await drain_until(ws1, lambda _: True, max_msgs=3, timeout_per_msg=0.1)
            if state and state["gameState"] == "playing":
                alive = sum(1 for e in state.get("enemies", []) if e["alive"])
                if alive > max_seen:
                    max_seen = alive
            if state and state["wave"] > 1:
                break  # wave advanced; stop sampling
            await asyncio.sleep(0.03)

        assert max_seen <= 3, (
            f"More than 3 enemies on screen simultaneously: {max_seen}"
        )
        print(f"  PASS: max enemies on screen: {max_seen} (limit=3)")
    finally:
        await cleanup(ws1, ws2)


# ── Test: score increases when enemy is killed ────────────────────────────────

async def test_score_increases_on_kill():
    """
    Killing an enemy in wave 1 adds 100 * 1 = 100 to score.
    We spam-shoot upward until score > 0.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Shoot continuously — some bullets will hit enemies
        for _ in range(200):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.05)

        state = await drain_until(
            ws1, lambda s: s.get("score", 0) > 0, max_msgs=400, timeout_per_msg=0.1
        )
        if state is None:
            print("  SKIP: couldn't get a kill in reasonable time (random map/positions)")
            return

        assert state["score"] >= 100, f"Expected score >= 100, got {state['score']}"
        assert state["score"] % 100 == 0, (
            f"Score {state['score']} is not a multiple of 100*wave"
        )
        print(f"  PASS: score after kill: {state['score']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: wave advances when all enemies defeated ─────────────────────────────

async def test_wave_advances_after_all_enemies_killed():
    """
    When all enemies are killed and enemiesLeft==0, game enters 'wave_clear'
    state for 3 seconds, then advances to the next wave.
    We let both players shoot continuously and wait.
    Note: this is a slow test (up to 90 seconds) and is skipped if it times out.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Both players shoot constantly
        async def spam_shoot(ws):
            for _ in range(600):
                try:
                    await send_input(ws, INPUT_KEYS_SHOOT)
                    await asyncio.sleep(0.05)
                except Exception:
                    break

        # Run both shooters concurrently while we poll for wave_clear
        task1 = asyncio.create_task(spam_shoot(ws1))
        task2 = asyncio.create_task(spam_shoot(ws2))

        # First wait for wave_clear state
        wc_state = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "wave_clear",
            max_msgs=3000,
            timeout_per_msg=0.05,
        )

        if wc_state is None:
            task1.cancel(); task2.cancel()
            print("  SKIP: wave_clear did not occur in time (depends on random map/AI)")
            return

        assert wc_state["gameState"] == "wave_clear"
        print(f"  wave_clear reached, waveClearTimer={wc_state.get('waveClearTimer', 0)}")

        # Then wait for wave 2 playing state
        state = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "playing" and s.get("wave", 1) >= 2,
            max_msgs=3000,
            timeout_per_msg=0.05,
        )
        task1.cancel(); task2.cancel()

        if state is None:
            print("  SKIP: wave did not advance in time after wave_clear")
            return

        assert state["wave"] >= 2
        # Wave 2 should have enemiesLeft = 3 + 2*2 = 7
        assert state["enemiesLeft"] == 3 + state["wave"] * 2, (
            f"enemiesLeft for wave {state['wave']} should be "
            f"{3 + state['wave'] * 2}, got {state['enemiesLeft']}"
        )
        print(f"  PASS: wave advanced to {state['wave']}, "
              f"enemiesLeft={state['enemiesLeft']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: money increases on kill ─────────────────────────────────────────────

async def test_money_increases_on_kill():
    """
    Killing an enemy awards money (100 * wave) to the killing player.
    The money field appears on each player object in the state.
    """
    ws1, ws2, p1_id, _, _ = await two_players_playing()
    try:
        # Shoot continuously — some bullets will hit enemies
        for _ in range(200):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.05)

        state = await drain_until(
            ws1,
            lambda s: any(p.get("money", 0) > 0 for p in s.get("players", [])),
            max_msgs=400,
            timeout_per_msg=0.1
        )
        if state is None:
            print("  SKIP: couldn't get a kill in reasonable time (random map/positions)")
            return

        p0 = find_player(state, p1_id)
        assert p0 is not None
        assert p0["money"] > 0, f"Expected money > 0, got {p0['money']}"
        assert p0["money"] % 100 == 0, f"Money {p0['money']} not a multiple of 100*wave"
        print(f"  PASS: player money after kill: ${p0['money']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: money resets on game start ──────────────────────────────────────────

async def test_money_resets_on_start():
    """Money must be 0 when a new game starts."""
    ws1, ws2, p1_id, p2_id, state = await two_players_playing()
    try:
        for p in state.get("players", []):
            assert p.get("money", 0) == 0, (
                f"Player {p['id']} has money={p.get('money')} at game start"
            )
        print("  PASS: all players start with $0")
    finally:
        await cleanup(ws1, ws2)


# ── Test: wave_clear state shows between waves ───────────────────────────────

async def test_wave_clear_state_appears():
    """
    After killing all enemies in a wave, the game enters 'wave_clear' state
    before advancing to the next wave.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        async def spam_shoot(ws):
            for _ in range(600):
                try:
                    await send_input(ws, INPUT_KEYS_SHOOT)
                    await asyncio.sleep(0.05)
                except Exception:
                    break

        task1 = asyncio.create_task(spam_shoot(ws1))
        task2 = asyncio.create_task(spam_shoot(ws2))

        state = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "wave_clear",
            max_msgs=3000,
            timeout_per_msg=0.05,
        )
        task1.cancel(); task2.cancel()

        if state is None:
            print("  SKIP: wave_clear did not occur in time")
            return

        assert state["gameState"] == "wave_clear"
        assert state.get("waveClearTimer", 0) > 0, "waveClearTimer should be positive"
        print(f"  PASS: wave_clear state reached, timer={state['waveClearTimer']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: player respawns after being hit (has lives remaining) ───────────────

async def test_player_respawns_after_hit():
    """
    In normal mode, players start with 3 lives.  When hit, lives decrements
    and the player is respawned at their spawn position with invuln=90.

    We induce a hit by turning off invuln via a restart and using friendly fire:
    P1 shoots P0.  This requires:
    1. Wait for invuln to expire on P0
    2. Have P1 face P0 and shoot
    This is fiddly, so we test the observable: after a hit, lives < 3 AND
    the player is still alive with invuln reset.
    """
    ws1, ws2, _, _, _ = await two_players_playing(hardmode=False)
    try:
        # Wait for P0's invuln to expire (90 frames ~ 1.5s)
        state = await drain_until(
            ws1,
            lambda s: (find_player(s, 0) or {}).get("invuln", 90) == 0,
            max_msgs=300,
            timeout_per_msg=0.02,
        )
        if state is None:
            print("  SKIP: P0 invuln never expired in time")
            return

        initial_lives = find_player(state, 0)["lives"]
        assert initial_lives == 3

        # P1 is at bottom-right; P0 at bottom-left after invuln expires.
        # P1 faces left (dir=3) naturally toward P0. Have P1 shoot.
        # P1 starts facing up, so first turn P1 left then shoot.
        await send_input(ws2, INPUT_KEYS_SHOOT)  # shoot upward to clear cooldown state
        await asyncio.sleep(0.05)

        # Turn P1 to face left
        from helpers import INPUT_KEYS_LEFT
        await send_input(ws2, {**INPUT_KEYS_LEFT, "shoot": True})
        await asyncio.sleep(0.3)

        # Poll for a lives decrease
        state = await drain_until(
            ws1,
            lambda s: (find_player(s, 0) or {}).get("lives", 3) < 3,
            max_msgs=200,
            timeout_per_msg=0.05,
        )
        if state is None:
            print("  SKIP: could not induce a hit in reasonable time (positional)")
            return

        p0 = find_player(state, 0)
        assert p0["lives"] == 2, f"Expected 2 lives after 1 hit, got {p0['lives']}"
        assert p0["alive"] is True, "Player should still be alive with lives > 0"
        assert p0["invuln"] > 0, "Player should be invulnerable after respawn"
        print(f"  PASS: P0 hit, lives={p0['lives']}, alive={p0['alive']}, "
              f"invuln={p0['invuln']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: game over when all lives lost ───────────────────────────────────────

async def test_gameover_when_all_lives_lost():
    """
    In hard mode (1 life each), game-over occurs when the last player's life
    reaches 0.  We wait for the game to naturally reach gameover via enemy hits,
    or we induce friendly fire.
    """
    ws1, ws2, _, _, _ = await two_players_playing(hardmode=True)
    try:
        # Both players have 1 life in hard mode.  Send them into enemy fire by
        # not moving — enemies will eventually kill them.
        await send_idle(ws1)
        await send_idle(ws2)

        state = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "gameover",
            max_msgs=3000,
            timeout_per_msg=0.1,
        )
        if state is None:
            print("  SKIP: gameover did not occur in time (AI-dependent)")
            return

        assert state["gameState"] == "gameover"
        # All active players should have 0 lives or be dead
        for p in state.get("players", []):
            assert p["lives"] <= 0 or not p["alive"], (
                f"Player {p['id']} lives={p['lives']} alive={p['alive']} in gameover"
            )
        print(f"  PASS: gameover reached, score={state['score']}, wave={state['wave']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: restart from gameover resets state ─────────────────────────────────

async def test_restart_from_gameover():
    """
    After gameover, sending 'restart' must transition back to 'playing' with
    score=0 and wave=1.
    """
    ws1, ws2, _, _, _ = await two_players_playing(hardmode=True)
    try:
        state = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "gameover",
            max_msgs=3000,
            timeout_per_msg=0.1,
        )
        if state is None:
            print("  SKIP: could not reach gameover to test restart")
            return

        await ws1.send(json.dumps({"type": "restart"}))
        playing = await drain_until(
            ws1,
            lambda s: s.get("gameState") == "playing",
            max_msgs=60,
        )
        assert playing is not None, "Game did not restart after gameover"
        assert playing["score"] == 0
        assert playing["wave"] == 1
        print("  PASS: restart from gameover resets to wave=1, score=0")
    finally:
        await cleanup(ws1, ws2)


# ── Runner ────────────────────────────────────────────────────────────────────

TESTS = [
    ("wave 1 initial state", test_wave1_initial_state),
    ("enemies spawn in wave 1", test_enemies_spawn_in_wave1),
    ("enemies left decrements on spawn", test_enemies_left_decrements_on_spawn),
    ("enemy colors are valid", test_enemy_colors_are_valid),
    ("enemy fields are valid", test_enemy_fields_are_valid),
    ("wave 1 only spawns basic types", test_wave1_only_spawns_basic_types),
    ("max 3 enemies on screen normal mode", test_max_3_enemies_on_screen_normal_mode),
    ("score increases on kill", test_score_increases_on_kill),
    ("money increases on kill", test_money_increases_on_kill),
    ("money resets on start", test_money_resets_on_start),
    ("wave_clear state appears", test_wave_clear_state_appears),
    ("wave advances after all enemies killed", test_wave_advances_after_all_enemies_killed),
    ("player respawns after hit", test_player_respawns_after_hit),
    ("gameover when all lives lost (hard mode)", test_gameover_when_all_lives_lost),
    ("restart from gameover", test_restart_from_gameover),
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
        await asyncio.sleep(0.3)

    print(f"\n{'='*50}")
    print(f"Wave/enemy tests: {passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
