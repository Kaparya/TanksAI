"""
Master test runner.

Runs all test suites sequentially against a live server.
Usage:
    source .venv/bin/activate
    python tests/run_tests.py

Or run individual suites:
    python tests/test_game_states.py
    python tests/test_movement.py
    python tests/test_shooting.py
    python tests/test_waves.py
    python tests/test_edge_cases.py

Requires:
    - Server running on ws://localhost:9001
    - Python packages: websockets (in .venv)
"""

import asyncio
import sys
import time

from test_multiplayer import test_multiplayer
import test_game_states
import test_movement
import test_shooting
import test_waves
import test_edge_cases


async def run_original():
    """Wrap the original monolithic test so it fits the suite runner interface."""
    await test_multiplayer()


SUITES = [
    ("Multiplayer (original)",  run_original),
    ("Game state transitions",  test_game_states.run_all),
    ("Movement & input",        test_movement.run_all),
    ("Shooting & bullets",      test_shooting.run_all),
    ("Waves & enemies",         test_waves.run_all),
    ("Edge cases & robustness", test_edge_cases.run_all),
]


async def main():
    print("=" * 60)
    print("TANKS GAME SERVER — FULL TEST SUITE")
    print("=" * 60)
    print("Server: ws://localhost:9001")
    print()

    suite_results = []
    total_start = time.monotonic()

    for suite_name, runner in SUITES:
        print(f"\n{'─'*60}")
        print(f"SUITE: {suite_name}")
        print('─' * 60)
        start = time.monotonic()
        status = "PASS"
        try:
            await runner()
        except SystemExit as e:
            if e.code != 0:
                status = "FAIL"
        except Exception as exc:
            print(f"  ERROR: {exc}")
            status = "FAIL"
        elapsed = time.monotonic() - start
        suite_results.append((suite_name, status, elapsed))
        # Pause between suites so server can finish processing disconnects
        await asyncio.sleep(0.5)

    total_elapsed = time.monotonic() - total_start

    print(f"\n{'='*60}")
    print("RESULTS SUMMARY")
    print('=' * 60)
    all_pass = True
    for name, status, secs in suite_results:
        icon = "PASS" if status == "PASS" else "FAIL"
        print(f"  [{icon}]  {name:<35}  {secs:.1f}s")
        if status != "PASS":
            all_pass = False

    print(f"\n  Total: {total_elapsed:.1f}s")
    if all_pass:
        print("\n  ALL SUITES PASSED")
    else:
        print("\n  SOME SUITES FAILED")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
