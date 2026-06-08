#!/usr/bin/env python3
"""Validate generated workspace tree without ROS2."""
from __future__ import annotations
import sys
from pathlib import Path

REQUIRED = [
    "src/low_speed_av_interfaces/package.xml",
    "src/low_speed_av_interfaces/CMakeLists.txt",
    "src/low_speed_av_interfaces/msg/TrajectoryPoint.msg",
    "src/low_speed_av_interfaces/msg/Trajectory.msg",
    "src/low_speed_av_interfaces/msg/GlobalRoute.msg",
    "src/low_speed_av_interfaces/msg/ControlCommand.msg",
    "src/low_speed_av_interfaces/srv/ReloadRoadnet.srv",
    "src/low_speed_av_planning/package.xml",
    "src/low_speed_av_planning/CMakeLists.txt",
    "src/low_speed_av_planning/config/planning_params.yaml",
    "src/low_speed_av_planning/launch/planning.launch.py",
    "src/low_speed_av_control/package.xml",
    "src/low_speed_av_control/CMakeLists.txt",
    "src/low_speed_av_control/config/control_params.yaml",
    "src/low_speed_av_control/launch/control.launch.py",
    "src/low_speed_av_bringup/package.xml",
    "src/low_speed_av_bringup/launch/planning_control_demo.launch.py",
]


def main() -> int:
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
    missing = [p for p in REQUIRED if not (root / p).exists()]
    if missing:
        print("Missing required generated files:")
        for p in missing:
            print(f"  - {p}")
        return 1
    print(f"Expected tree OK: {root}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
