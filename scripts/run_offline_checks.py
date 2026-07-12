#!/usr/bin/env python3
"""Canonical cross-platform runner for non-ROS2 data and consistency checks."""

from __future__ import annotations

import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class Check:
    name: str
    command: tuple[str, ...]
    required: bool = True


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    python = Path(sys.executable).resolve()
    print(f"offline runner python={python}")
    print(f"offline runner root={root}")
    checks = [
        Check("expected_tree", ("scripts/validate_expected_tree.py",)),
        Check("sample_package", ("scripts/validate_sample_ad_package.py", "src/low_speed_av_bringup/sample_ad_package")),
        Check("formal_package_1", ("scripts/validate_sample_ad_package.py", "roadnet_ad_package_20260610T012525Z_1")),
        Check("formal_package_2", ("scripts/validate_sample_ad_package.py", "roadnet_ad_package_20260610T012525Z_2")),
        Check("algorithm_sample", ("scripts/offline_algorithm_smoke.py", "src/low_speed_av_bringup/sample_ad_package")),
        Check("python_entrypoint_regressions", ("scripts/offline_entrypoint_regression_smoke.py",)),
        Check("remaining_fixes", ("scripts/offline_remaining_fixes_smoke.py",)),
        Check("reverse_policy", ("scripts/offline_reverse_policy_smoke.py",)),
        Check("runtime_followup", ("scripts/offline_runtime_followup_smoke.py", "roadnet_ad_package_20260610T012525Z_2")),
        Check("scu_lqr", ("scripts/offline_scu_lqr_smoke.py",)),
        Check("semantic_goal", ("scripts/offline_semantic_goal_followup_smoke.py",)),
        Check("sim_localization", ("scripts/offline_sim_localization_follow_smoke.py",)),
        Check("simulation", ("scripts/offline_simulation_smoke.py", "roadnet_ad_package_20260610T012525Z_2")),
        Check("trajectory_continuity", ("scripts/offline_trajectory_continuity_smoke.py",)),
        Check("phase13_safety_contract", ("scripts/offline_phase13_safety_smoke.py",)),
        Check("control_config_contract", ("scripts/check_control_config_contract.py",)),
        Check("template_consistency", ("scripts/check_template_consistency.py",)),
        Check("repository_hygiene", ("scripts/offline_repository_hygiene.py",)),
    ]
    passed = failed = skipped = 0
    for check in checks:
        command = [str(python), *check.command]
        print(f"\n[RUN] {check.name}: {' '.join(command)}", flush=True)
        result = subprocess.run(command, cwd=root, check=False)
        if result.returncode == 0:
            passed += 1
            print(f"[PASS] {check.name}")
        elif check.required:
            failed += 1
            print(f"[FAIL] {check.name}: exit={result.returncode}")
        else:
            skipped += 1
            print(f"[SKIPPED] {check.name}: exit={result.returncode}")
    total = len(checks)
    print(f"\nSUMMARY total={total} pass={passed} fail={failed} skipped={skipped}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
