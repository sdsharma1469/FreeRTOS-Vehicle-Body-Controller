#!/usr/bin/env python3

import subprocess
import sys


EXPECTED = {
    "nominal": {
        "mode": "DRIVE",
        "fault": "0",
        "locked": "1",
        "interior": "0",
        "headlights": "1",
        "hazards": "0",
    },
    "door_open": {
        "mode": "DRIVE",
        "fault": "0",
        "locked": "0",
        "interior": "1",
        "headlights": "0",
        "hazards": "0",
    },
    "stale_inputs": {
        "mode": "FAULT",
        "fault": "1",
        "locked": "0",
        "interior": "0",
        "headlights": "0",
        "hazards": "1",
    },
    "bad_counter": {
        "mode": "FAULT",
        "fault": "1",
        "locked": "0",
        "interior": "0",
        "headlights": "0",
        "hazards": "0",
    },
}


def parse(line: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for token in line.strip().split():
        key, value = token.split("=", 1)
        result[key] = value
    return result


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: run_scenarios.py PATH_TO_SIM", file=sys.stderr)
        return 2

    binary = sys.argv[1]
    failed = False

    for scenario, expected in EXPECTED.items():
        completed = subprocess.run(
            [binary, scenario], check=False, text=True, capture_output=True
        )
        if completed.returncode != 0:
            print(f"FAIL {scenario}: simulator exited {completed.returncode}")
            print(completed.stderr)
            failed = True
            continue

        actual = parse(completed.stdout)
        mismatches = {
            key: (expected_value, actual.get(key))
            for key, expected_value in expected.items()
            if actual.get(key) != expected_value
        }

        if mismatches:
            print(f"FAIL {scenario}: {mismatches}")
            failed = True
        else:
            print(f"PASS {scenario}: {completed.stdout.strip()}")

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
