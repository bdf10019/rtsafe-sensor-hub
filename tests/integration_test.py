#!/usr/bin/env python3
"""End-to-end simulator and supervisor verification using only the standard library."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys


def run(command: list[str], expected_code: int = 0) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode != expected_code:
        raise AssertionError(
            f"command returned {result.returncode}, expected {expected_code}: {command}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return result


def load_json(path: Path) -> dict[str, object]:
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def simulate(simulator: Path, root: Path, scenario: str) -> tuple[Path, dict[str, object]]:
    output = root / scenario
    run(
        [
            str(simulator),
            "--scenario",
            scenario,
            "--cycles",
            "2000",
            "--seed",
            "424242",
            "--output-dir",
            str(output),
        ]
    )
    return output, load_json(output / "summary.json")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--simulator", type=Path, required=True)
    parser.add_argument("--supervisor", type=Path, required=True)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()

    if args.work_dir.exists():
        shutil.rmtree(args.work_dir)
    args.work_dir.mkdir(parents=True)

    nominal_dir, nominal = simulate(args.simulator, args.work_dir, "nominal")
    assert nominal["final_state"] == "NORMAL"
    assert nominal["degraded_observed"] is False
    assert nominal["safe_stop_observed"] is False

    nominal_report = nominal_dir / "supervisor.json"
    run(
        [
            str(args.supervisor),
            "--input",
            str(nominal_dir / "telemetry.bin"),
            "--json",
            str(nominal_report),
        ]
    )
    report = load_json(nominal_report)
    assert report["valid_frames"] == 2000
    assert report["invalid_frames"] == 0
    assert report["sequence_gaps"] == 0
    assert report["stream_valid"] is True

    _, dropout = simulate(args.simulator, args.work_dir, "imu-dropout")
    assert dropout["degraded_observed"] is True
    assert dropout["safe_stop_observed"] is False
    assert dropout["final_state"] == "NORMAL"

    _, force = simulate(args.simulator, args.work_dir, "force-spike")
    assert force["safe_stop_observed"] is True
    assert force["first_safe_stop_cycle"] == force["first_fault_cycle"]

    _, heartbeat = simulate(args.simulator, args.work_dir, "heartbeat-loss")
    assert heartbeat["safe_stop_observed"] is True
    assert heartbeat["final_state"] == "SAFE_STOP"

    _, deadline = simulate(args.simulator, args.work_dir, "deadline-overrun")
    assert deadline["deadline_misses"] == 5
    assert deadline["degraded_observed"] is True
    assert deadline["safe_stop_observed"] is False

    corrupted_dir, corrupted = simulate(
        args.simulator, args.work_dir, "crc-corruption"
    )
    assert corrupted["invalid_frames_injected"] == 1
    corrupted_report = corrupted_dir / "supervisor.json"
    run(
        [
            str(args.supervisor),
            "--input",
            str(corrupted_dir / "telemetry.bin"),
            "--json",
            str(corrupted_report),
        ],
        expected_code=2,
    )
    report = load_json(corrupted_report)
    assert report["invalid_frames"] == 1
    assert report["stream_valid"] is False

    print("All end-to-end scenarios passed")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"integration failure: {error}", file=sys.stderr)
        raise SystemExit(1)

