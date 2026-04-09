"""Test multiplayer functionality of the tanks game server."""

import asyncio
import json
import websockets


async def drain_until(ws, condition, max_msgs=60):
    """Keep reading until condition(state) is true or max_msgs reached."""
    for _ in range(max_msgs):
        msg = await asyncio.wait_for(ws.recv(), timeout=2)
        state = json.loads(msg)
        if state.get("type") == "state" and condition(state):
            return state
    return None


async def test_multiplayer():
    # Player 1 connects
    ws1 = await websockets.connect("ws://localhost:9001")
    msg = await ws1.recv()
    data = json.loads(msg)
    print("P1 welcome:", data)
    assert data["type"] == "welcome" and data["playerId"] == 0

    # Player 2 connects
    ws2 = await websockets.connect("ws://localhost:9001")
    msg = await ws2.recv()
    data = json.loads(msg)
    print("P2 welcome:", data)
    assert data["type"] == "welcome" and data["playerId"] == 1

    # Start game
    await ws1.send(json.dumps({"type": "start", "hardmode": False}))
    state = await drain_until(ws1, lambda s: s.get("gameState") == "playing")
    assert state, "Never got playing state"
    print(f"Playing: {len(state['players'])} players")
    assert len(state["players"]) == 2
    assert state["players"][0]["color"] == "#00e676"
    assert state["players"][1]["color"] == "#00bcd4"
    assert state["players"][0]["lives"] == 3
    assert state["players"][1]["lives"] == 3
    assert "lives" not in state  # no top-level lives
    print("  Protocol OK: players array, no top-level lives")

    # Player 3 (rejected)
    ws3 = await websockets.connect("ws://localhost:9001")
    msg = await ws3.recv()
    data = json.loads(msg)
    print(f"P3 rejected: {data}")
    assert data["type"] == "full"

    # Test input (player 1 moves up)
    await ws1.send(
        json.dumps(
            {
                "type": "input",
                "keys": {
                    "up": True,
                    "down": False,
                    "left": False,
                    "right": False,
                    "shoot": False,
                },
            }
        )
    )
    await asyncio.sleep(0.2)
    state = await drain_until(ws1, lambda s: True)
    p1_y = state["players"][0]["y"]
    print(f"P1 position after up input: y={p1_y:.1f} (started at 500)")
    await ws1.send(
        json.dumps(
            {
                "type": "input",
                "keys": {
                    "up": False,
                    "down": False,
                    "left": False,
                    "right": False,
                    "shoot": False,
                },
            }
        )
    )

    # P2 disconnects
    await ws2.close()
    state = await drain_until(
        ws1, lambda s: len(s.get("players", [])) == 1
    )
    assert state, "Disconnect not reflected in state"
    print(f"After P2 disconnect: {len(state['players'])} player(s)")

    # P3 slot freed, can reconnect
    ws3b = await websockets.connect("ws://localhost:9001")
    msg = await ws3b.recv()
    data = json.loads(msg)
    print(f"P3 retry: {data}")
    assert data["type"] == "welcome" and data["playerId"] == 1

    await ws1.close()
    await ws3.close()
    await ws3b.close()
    print()
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    asyncio.run(test_multiplayer())
