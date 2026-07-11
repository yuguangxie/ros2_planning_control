#!/usr/bin/env python3
"""Regression checks for default package paths and empty semantic fixtures."""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(*arguments: str) -> None:
    result = subprocess.run([sys.executable, *arguments], cwd=ROOT, check=False)
    if result.returncode != 0:
        raise AssertionError(f"command failed ({result.returncode}): {arguments}")


def main() -> int:
    run("scripts/offline_runtime_followup_smoke.py")
    run("scripts/offline_simulation_smoke.py")
    with tempfile.TemporaryDirectory(prefix="low_speed_av_empty_parking_") as temp:
        fixture = Path(temp) / "sample"
        shutil.copytree(ROOT / "src/low_speed_av_bringup/sample_ad_package", fixture)
        parking_path = fixture / "semantics/parking_points.json"
        parking = json.loads(parking_path.read_text(encoding="utf-8"))
        parking["parking_points"] = []
        parking_path.write_text(json.dumps(parking, ensure_ascii=False, indent=2), encoding="utf-8")
        run("scripts/offline_algorithm_smoke.py", str(fixture))
    print("Offline entrypoint regression OK: default paths valid, empty parking_points handled")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
