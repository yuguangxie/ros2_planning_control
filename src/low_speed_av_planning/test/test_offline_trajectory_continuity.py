#!/usr/bin/env python3
"""colcon-test entry point for offline trajectory continuity checks."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def test_offline_trajectory_continuity_smoke() -> None:
    repo_root = Path.cwd()
    script = repo_root / "scripts" / "offline_trajectory_continuity_smoke.py"
    result = subprocess.run(
        [sys.executable, str(script)],
        cwd=repo_root,
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert result.returncode == 0, result.stdout
