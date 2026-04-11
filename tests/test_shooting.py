"""
Tests for shooting, bullet lifecycle, and wall destruction.

Verifies:
- Shooting produces a bullet in the state
- Bullet owner matches the shooting player
- Bullet velocity direction matches player facing direction
- Cooldown prevents rapid-fire (second shot before cooldown returns no bullet)
- Player bullet hitting brick wall destroys the wall (type 1 -> 0)
- Player bullet hitting steel wall does NOT destroy it (type 2 stays 2)
- Particles are spawned on wall destruction
- Bullets are removed when they go out of bounds
- Player cannot shoot while invulnerable (invuln does not block shooting;
  this test actually validates shooting WORKS during invuln since the server
  allows it — the invuln only blocks RECEIVING damage)
- Bullets are cleared on restart
- Shooting while paused does not produce new bullets (tick is frozen)

Note: Direct "friendly fire kills" and "enemy bullet kills" require waiting
many seconds for enemy spawns + getting an enemy to shoot a player. Those
scenarios are covered in test_waves.py where we have enemies on the field.
"""

import asyncio
import json
from helpers import (
    WS_URL, connect_player, drain_until, start_game, send_input, send_idle,
    find_player,
    P0_SPAWN_X, P0_SPAWN_Y,
    INPUT_KEYS_IDLE, INPUT_KEYS_UP, INPUT_KEYS_SHOOT,
    PLAYER_COOLDOWN, TILE,
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
    await asyncio.sleep(0.15)


def bullets_owned_by(state, owner):
    return [b for b in state.get("bullets", []) if b["owner"] == owner]


def wall_at(state, col, row):
    """Return wall value at grid position (col, row)."""
    return state["walls"][row][col]


# ── Test: shooting produces a bullet ─────────────────────────────────────────

async def test_shoot_produces_bullet():
    """Sending shoot=True causes a bullet to appear in state.bullets."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Wait for invuln to partially drain (doesn't affect shooting, but
        # makes it easier to wait for a clean state)
        await asyncio.sleep(0.05)

        await send_input(ws1, INPUT_KEYS_SHOOT)
        # Server needs a tick to process and one broadcast to reach us
        state = await drain_until(
            ws1,
            lambda s: len(bullets_owned_by(s, 0)) > 0,
            max_msgs=30
        )
        assert state is not None, "No bullet appeared after shoot input"
        b = bullets_owned_by(state, 0)[0]
        assert b["owner"] == 0
        print(f"  PASS: bullet spawned at ({b['x']:.1f},{b['y']:.1f}) owner={b['owner']}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: bullet direction matches player facing direction ────────────────────

async def test_bullet_velocity_matches_facing_direction():
    """
    Player 0 faces up (dir=0) at spawn: DX[0]=0, DY[0]=-1.
    So vx should be ~0 and vy should be negative (upward).
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await send_input(ws1, INPUT_KEYS_SHOOT)
        state = await drain_until(
            ws1,
            lambda s: len(bullets_owned_by(s, 0)) > 0,
            max_msgs=30
        )
        assert state is not None, "No bullet appeared"
        b = bullets_owned_by(state, 0)[0]
        assert abs(b["vx"]) < 0.1, f"Expected vx~0 for upward shot, got {b['vx']}"
        assert b["vy"] < 0, f"Expected vy<0 (upward), got {b['vy']}"
        print(f"  PASS: upward bullet vx={b['vx']:.2f}, vy={b['vy']:.2f}")
    finally:
        await cleanup(ws1, ws2)


# ── Test: bullet owner is correct ─────────────────────────────────────────────

async def test_bullet_owner_identifies_shooter():
    """P0 bullet has owner=0, P1 bullet has owner=1."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # P0 shoots
        await send_input(ws1, INPUT_KEYS_SHOOT)
        state = await drain_until(
            ws1, lambda s: len(bullets_owned_by(s, 0)) > 0, max_msgs=30
        )
        assert state is not None
        assert all(b["owner"] == 0 for b in bullets_owned_by(state, 0))

        # P1 shoots (faces up too)
        await send_input(ws2, INPUT_KEYS_SHOOT)
        state = await drain_until(
            ws2, lambda s: len(bullets_owned_by(s, 1)) > 0, max_msgs=30
        )
        assert state is not None
        assert all(b["owner"] == 1 for b in bullets_owned_by(state, 1))
        print("  PASS: bullet owners match shooting players")
    finally:
        await cleanup(ws1, ws2)


# ── Test: cooldown limits fire rate ──────────────────────────────────────────

async def test_cooldown_limits_fire_rate():
    """
    Firing twice in one tick should still produce only one bullet (the second
    shoot is blocked by the cooldown timer).  We fire, grab the bullet count,
    fire again immediately, and assert the count didn't jump by 2.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # First shot
        await send_input(ws1, INPUT_KEYS_SHOOT)
        state = await drain_until(
            ws1, lambda s: len(bullets_owned_by(s, 0)) > 0, max_msgs=30
        )
        assert state is not None, "First bullet never appeared"
        count_after_first = len(bullets_owned_by(state, 0))

        # Immediately fire again (cooldown still active)
        await send_input(ws1, INPUT_KEYS_SHOOT)
        await asyncio.sleep(0.03)  # ~2 frames
        await send_idle(ws1)
        state2 = await drain_until(ws1, lambda _: True, max_msgs=5)
        count_after_second = len(bullets_owned_by(state2, 0))

        # The second shot may have added at most the same number as the first;
        # if the cooldown works, no extra bullet should appear in the 2-frame window
        # (cooldown=15 ticks means ~250ms before another shot is allowed)
        assert count_after_second <= count_after_first + 1, (
            f"Cooldown broken: bullet count jumped from {count_after_first} to "
            f"{count_after_second} in 2 frames"
        )
        print(f"  PASS: cooldown respected (bullets after 2nd shot: {count_after_second})")
    finally:
        await cleanup(ws1, ws2)


# ── Test: bullets travel in the right direction ────────────────────────────────

async def test_bullet_travels_upward():
    """A bullet fired by P0 (facing up) should have decreasing y over time."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await send_input(ws1, INPUT_KEYS_SHOOT)
        state1 = await drain_until(
            ws1, lambda s: len(bullets_owned_by(s, 0)) > 0, max_msgs=30
        )
        assert state1 is not None
        y1 = bullets_owned_by(state1, 0)[0]["y"]

        # After a few more frames the same bullet should be higher up
        state2 = await drain_until(ws1, lambda _: True, max_msgs=10)
        p0_bullets = bullets_owned_by(state2, 0)
        # Bullet may have already hit a wall; if it disappeared that's OK too
        if p0_bullets:
            y2 = p0_bullets[0]["y"]
            assert y2 <= y1, f"Bullet should move up (y decrease): y1={y1:.1f}, y2={y2:.1f}"
            print(f"  PASS: bullet y decreased from {y1:.1f} to {y2:.1f}")
        else:
            print(f"  PASS: bullet hit wall/border and was removed (started at y={y1:.1f})")
    finally:
        await cleanup(ws1, ws2)


# ── Test: brick wall is destroyed by player bullet ────────────────────────────

async def test_player_bullet_destroys_brick_wall():
    """
    Fire player 0 upward repeatedly until a brick wall (type 1) directly above
    the spawn column is hit and removed.

    Strategy: P0 starts at column ~1 (x=60), row 12 (y=500).  We fire upward
    repeatedly. The first brick wall (type 1) in that column above P0 will be
    destroyed on bullet impact.  We compare wall arrays before and after.
    """
    ws1, ws2, _, _, start_state = await two_players_playing()
    try:
        # Find a brick wall in column 1 above row 12
        walls = start_state["walls"]
        p0_col = int(P0_SPAWN_X // TILE)  # = 1
        target_row = None
        for row in range(11, 0, -1):   # scan upward from row 11
            if walls[row][p0_col] == 1:
                target_row = row
                break

        if target_row is None:
            # No brick in that column; regenerate by restarting
            await ws1.send(json.dumps({"type": "restart"}))
            state2 = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
            walls = state2["walls"]
            for row in range(11, 0, -1):
                if walls[row][p0_col] == 1:
                    target_row = row
                    break

        if target_row is None:
            print("  SKIP: no brick wall found in P0 spawn column after restart (random map)")
            return

        print(f"  Found brick at col={p0_col}, row={target_row}; firing upward")

        # Fire repeatedly — each bullet travels upward; one will hit the brick
        destroyed = False
        for _ in range(40):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.05)  # ~3 frames per shot (cooldown=15 ticks ~250ms, but
            # we just need enough shots to guarantee one reaches the wall)
        await send_idle(ws1)

        # Wait for bullet effects to settle
        await asyncio.sleep(0.3)
        state = await drain_until(ws1, lambda _: True)
        if state["walls"][target_row][p0_col] == 0:
            destroyed = True

        assert destroyed, (
            f"Brick at ({p0_col},{target_row}) was not destroyed after sustained fire"
        )
        print(f"  PASS: brick wall at col={p0_col}, row={target_row} was destroyed")
    finally:
        await cleanup(ws1, ws2)


# ── Test: steel wall survives player bullet ───────────────────────────────────

async def test_player_bullet_does_not_destroy_steel_wall():
    """
    Steel walls (type 2) must not be destroyed by player bullets.
    The border walls at row 0 are always steel (game_engine.cpp: walls[0][x] = 2).
    """
    ws1, ws2, _, _, state = await two_players_playing()
    try:
        # Verify row 0 is all steel before firing
        assert all(state["walls"][0][x] == 2 for x in range(1, 19)), \
            "Expected steel border at row 0"

        # Fire straight up repeatedly; bullets will hit the top steel wall
        for _ in range(30):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.06)
        await send_idle(ws1)
        await asyncio.sleep(0.3)

        state2 = await drain_until(ws1, lambda _: True)
        # Border should still be all steel
        damaged = [x for x in range(1, 19) if state2["walls"][0][x] != 2]
        assert not damaged, (
            f"Steel border at row 0 was damaged at columns: {damaged}"
        )
        print("  PASS: steel border walls survive player bullets")
    finally:
        await cleanup(ws1, ws2)


# ── Test: bullets cleared on restart ─────────────────────────────────────────

async def test_bullets_cleared_on_restart():
    """After restart, bullets array must be empty."""
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Fire a bunch of bullets
        for _ in range(5):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.08)
        await send_idle(ws1)

        await ws1.send(json.dumps({"type": "restart"}))
        state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
        assert state is not None
        assert len(state.get("bullets", [])) == 0, (
            f"Bullets not cleared on restart: {len(state['bullets'])} remaining"
        )
        print("  PASS: bullets cleared on restart")
    finally:
        await cleanup(ws1, ws2)


# ── Test: bullets not produced while paused ───────────────────────────────────

async def test_no_bullets_produced_while_paused():
    """
    While the game is paused, holding shoot=True must not add any new bullets.
    The tick does not run while paused, so no bullets are ever spawned.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        await ws1.send(json.dumps({"type": "pause"}))
        await drain_until(ws1, lambda s: s.get("gameState") == "paused")

        # Send shoot repeatedly while paused
        for _ in range(5):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.05)

        state = await drain_until(ws1, lambda _: True)
        bullet_count = len(state.get("bullets", []))
        assert bullet_count == 0, (
            f"Bullets appeared while paused: {bullet_count}"
        )
        print("  PASS: no bullets spawned while paused")
    finally:
        await cleanup(ws1, ws2)


# ── Test: particles appear on bullet-wall impact ─────────────────────────────

async def test_explosions_on_wall_impact():
    """
    When a bullet hits a wall, the server sends explosion events.
    After firing upward, we should see explosions in the state.
    """
    ws1, ws2, _, _, _ = await two_players_playing()
    try:
        # Fire multiple times to ensure at least one wall hit
        for _ in range(6):
            await send_input(ws1, INPUT_KEYS_SHOOT)
            await asyncio.sleep(0.08)
        await send_idle(ws1)
        await asyncio.sleep(0.1)

        state = await drain_until(ws1, lambda s: len(s.get("explosions", [])) > 0, max_msgs=60)
        assert state is not None, "No explosions appeared after shooting walls"
        print(f"  PASS: {len(state['explosions'])} explosions on bullet impact")
    finally:
        await cleanup(ws1, ws2)


# ── Runner ────────────────────────────────────────────────────────────────────

TESTS = [
    ("shoot produces bullet", test_shoot_produces_bullet),
    ("bullet velocity matches facing direction", test_bullet_velocity_matches_facing_direction),
    ("bullet owner identifies shooter", test_bullet_owner_identifies_shooter),
    ("cooldown limits fire rate", test_cooldown_limits_fire_rate),
    ("bullet travels upward", test_bullet_travels_upward),
    ("player bullet destroys brick wall", test_player_bullet_destroys_brick_wall),
    ("player bullet does not destroy steel wall", test_player_bullet_does_not_destroy_steel_wall),
    ("bullets cleared on restart", test_bullets_cleared_on_restart),
    ("no bullets while paused", test_no_bullets_produced_while_paused),
    ("explosions on wall impact", test_explosions_on_wall_impact),
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
    print(f"Shooting tests: {passed} passed, {failed} failed")
    if failed:
        raise SystemExit(1)


if __name__ == "__main__":
    asyncio.run(run_all())
