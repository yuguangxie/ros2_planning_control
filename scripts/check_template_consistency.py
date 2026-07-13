#!/usr/bin/env python3
"""Verify canonical scripts/config/sample relationships without ROS2."""

from __future__ import annotations

import hashlib
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]

# Config templates intentionally have no package-specific differences. If a
# future difference is needed it must be listed here with a reason and handled
# explicitly instead of weakening the comparison.
ALLOWED_CONFIG_DIFFERENCES: dict[str, str] = {}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def normalized_script(path: Path) -> str:
    text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
    return text.replace("templates/sample_ad_package", "src/low_speed_av_bringup/sample_ad_package")


def compare_scripts() -> None:
    for name in (
        "validate_sample_ad_package.py",
        "validate_expected_tree.py",
        "offline_algorithm_smoke.py",
    ):
        runtime = normalized_script(ROOT / "scripts" / name)
        template = normalized_script(ROOT / "templates/offline_validation" / name)
        if runtime != template:
            raise AssertionError(f"template validator drift: {name}")


def compare_configs() -> None:
    pairs = {
        "control": (
            ROOT / "src/low_speed_av_control/config/control_params.yaml",
            ROOT / "templates/sample_config/control_params.yaml",
        ),
        "planning": (
            ROOT / "src/low_speed_av_planning/config/planning_params.yaml",
            ROOT / "templates/sample_config/planning_params.yaml",
        ),
    }
    for name, (production, template) in pairs.items():
        prod_data = yaml.safe_load(production.read_text(encoding="utf-8"))
        template_data = yaml.safe_load(template.read_text(encoding="utf-8"))
        if prod_data != template_data:
            raise AssertionError(
                f"{name} config template differs from canonical production config; "
                f"allowlist={ALLOWED_CONFIG_DIFFERENCES}"
            )


def compare_sample_packages() -> None:
    canonical = ROOT / "templates/sample_ad_package"
    generated = ROOT / "src/low_speed_av_bringup/sample_ad_package"
    canonical_files = sorted(path.relative_to(canonical) for path in canonical.rglob("*") if path.is_file())
    generated_files = sorted(path.relative_to(generated) for path in generated.rglob("*") if path.is_file())
    if canonical_files != generated_files:
        raise AssertionError("sample AD Package file list drift")
    mismatches = [
        str(relative) for relative in canonical_files
        if sha256(canonical / relative) != sha256(generated / relative)
    ]
    if mismatches:
        raise AssertionError(f"sample AD Package hash drift: {mismatches}")


def check_required_tree() -> None:
    required = [
        "src/low_speed_av_interfaces/srv/PlanMission.srv",
        "src/low_speed_av_simulation/src/roadnet_visualization_node.cpp",
        "src/low_speed_av_simulation/test/test_sim_localization_reset_launch.py",
        "src/low_speed_av_simulation/test/test_simulation_core.cpp",
        "src/low_speed_av_simulation/test/test_control_closed_loop_plant_launch.py",
        "src/low_speed_av_bringup/launch/planning_control_closed_loop_sim.launch.py",
        "src/low_speed_av_bringup/test/test_planning_control_closed_loop_sil_launch.py",
        "src/yunle_chassis/chassis_driver/src/scu_control_frame_builder.cpp",
        "src/yunle_chassis/chassis_driver/test/test_chassis_core.cpp",
        "src/low_speed_av_bringup/test/test_planning_control_safety_launch.py",
        "src/low_speed_av_bringup/test/test_control_runtime_launch.py",
        "src/low_speed_av_control/src/control_runtime_helpers.cpp",
        "src/low_speed_av_control/test/test_control_runtime_helpers.cpp",
        "scripts/check_control_config_contract.py",
        "scripts/run_offline_checks.py",
        ".github/workflows/ros2_humble_ci.yml",
    ]
    missing = [relative for relative in required if not (ROOT / relative).exists()]
    if missing:
        raise AssertionError(f"required Phase 14 tree entries missing: {missing}")


def main() -> int:
    compare_scripts()
    compare_configs()
    compare_sample_packages()
    check_required_tree()
    print(
        "Template consistency OK: runtime validators canonical, configs synchronized, "
        "sample AD Package hashes equal, Phase 18 tree complete"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
