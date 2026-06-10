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
    "src/low_speed_av_interfaces/msg/VehicleState.msg",
    "src/low_speed_av_interfaces/msg/ModuleStatus.msg",
    "src/low_speed_av_interfaces/msg/RoadnetStatus.msg",
    "src/low_speed_av_interfaces/srv/ReloadRoadnet.srv",
    "src/low_speed_av_interfaces/srv/PlanRoute.srv",
    "src/low_speed_av_interfaces/srv/SetPlannerAlgorithm.srv",
    "src/low_speed_av_interfaces/srv/SetControllerAlgorithm.srv",
    "src/low_speed_av_planning/package.xml",
    "src/low_speed_av_planning/CMakeLists.txt",
    "src/low_speed_av_planning/include/low_speed_av_planning/roadnet_loader.hpp",
    "src/low_speed_av_planning/include/low_speed_av_planning/dijkstra_planner.hpp",
    "src/low_speed_av_planning/include/low_speed_av_planning/astar_planner.hpp",
    "src/low_speed_av_planning/include/low_speed_av_planning/reference_line_motion_planner.hpp",
    "src/low_speed_av_planning/include/low_speed_av_planning/curvature_speed_planner.hpp",
    "src/low_speed_av_planning/config/planning_params.yaml",
    "src/low_speed_av_planning/launch/planning.launch.py",
    "src/low_speed_av_control/package.xml",
    "src/low_speed_av_control/CMakeLists.txt",
    "src/low_speed_av_control/include/low_speed_av_control/front_ackermann_model.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/dual_ackermann_model.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/pure_pursuit_controller.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/stanley_controller.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/command_limiter.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/command_smoother.hpp",
    "src/low_speed_av_control/include/low_speed_av_control/scu_command_mapper.hpp",
    "src/low_speed_av_control/config/control_params.yaml",
    "src/low_speed_av_control/launch/control.launch.py",
    "src/low_speed_av_simulation/package.xml",
    "src/low_speed_av_simulation/CMakeLists.txt",
    "src/low_speed_av_simulation/src/roadnet_visualization_node.cpp",
    "src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp",
    "src/low_speed_av_simulation/config/simulation_params.yaml",
    "src/low_speed_av_simulation/launch/simulation_visualization.launch.py",
    "src/low_speed_av_simulation/rviz/roadnet_simulation.rviz",
    "src/low_speed_av_bringup/package.xml",
    "src/low_speed_av_bringup/launch/planning_control_demo.launch.py",
    "src/low_speed_av_bringup/sample_ad_package/project_manifest.json",
    "src/low_speed_av_bringup/sample_ad_package/trajectory/waypoints.yaml",
    "src/low_speed_av_bringup/sample_ad_package/validation/validation_report.json",
    "scripts/validate_sample_ad_package.py",
    "scripts/offline_algorithm_smoke.py",
    "scripts/offline_simulation_smoke.py",
    "scripts/offline_scu_lqr_smoke.py",
    "docs/YUNLE_SCU_COMMAND_OUTPUT.md",
    "docs/LQR_CONTROLLER_DESIGN.md",
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
