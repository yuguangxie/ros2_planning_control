# 09 Work Plan

## Phase 00: Discovery

Goal: scan repository, detect existing structure, create implementation plan and phase report.

Deliverable:

```text
reports/phase_00_report.md
```

## Phase 01: Interfaces

Goal: generate `low_speed_av_interfaces` package with messages and services.

Deliverables:

```text
src/low_speed_av_interfaces/msg/*.msg
src/low_speed_av_interfaces/srv/*.srv
src/low_speed_av_interfaces/CMakeLists.txt
src/low_speed_av_interfaces/package.xml
reports/phase_01_report.md
```

## Phase 02: AD Package Loader

Goal: implement loader for `project_manifest.json`, `topology.json`, `waypoints.yaml`, `waypoint_index.json`, semantics and validation.

Deliverables:

```text
src/low_speed_av_planning/include/.../roadnet_loader.hpp
src/low_speed_av_planning/src/roadnet_loader.cpp
scripts/validate_sample_ad_package.py
reports/phase_02_report.md
```

## Phase 03: Global Planner

Goal: implement Dijkstra and A* over topology edges.

Deliverables:

```text
global_planner_base.hpp
dijkstra_planner.hpp/cpp
astar_planner.hpp/cpp
global_planner_factory.hpp/cpp
reports/phase_03_report.md
```

## Phase 04: Motion and Speed Planner

Goal: stitch edge waypoints and generate local trajectory with selectable speed planner.

Deliverables:

```text
motion_planner_base.hpp
reference_line_motion_planner.hpp/cpp
speed_planner_base.hpp
curvature_speed_planner.hpp/cpp
reports/phase_04_report.md
```

## Phase 05: Planning Node Integration

Goal: implement ROS2 planning node, parameters, services, publishers and status.

Deliverables:

```text
planning_node.hpp/cpp
config/planning_params.yaml
launch/planning.launch.py
reports/phase_05_report.md
```

## Phase 06: Vehicle Model and Control Interfaces

Goal: implement front and dual Ackermann model, command limiter, internal control data types.

Deliverables:

```text
vehicle_model.hpp/cpp
front_ackermann_model.hpp/cpp
dual_ackermann_model.hpp/cpp
command_limiter.hpp/cpp
reports/phase_06_report.md
```

## Phase 07: Control Algorithms

Goal: implement Pure Pursuit, Stanley, LQR and MPC sampler.

Deliverables:

```text
controller_base.hpp
pure_pursuit_controller.hpp/cpp
stanley_controller.hpp/cpp
lqr_controller.hpp/cpp
mpc_sampler_controller.hpp/cpp
controller_factory.hpp/cpp
reports/phase_07_report.md
```

## Phase 08: Control Node and Safety

Goal: implement ROS2 control node, timeout handling, smoother and safety stop.

Deliverables:

```text
control_node.hpp/cpp
command_smoother.hpp/cpp
config/control_params.yaml
launch/control.launch.py
reports/phase_08_report.md
```

## Phase 09: Bringup and Docs

Goal: generate bringup package, configs, launch demo, README and operation guide.

Deliverables:

```text
src/low_speed_av_bringup/*
docs/runtime_usage.md
reports/phase_09_report.md
```

## Phase 10: No-ROS2 Tests and Acceptance

Goal: run offline scripts and create acceptance report.

Deliverables:

```text
scripts/validate_expected_tree.py
scripts/validate_sample_ad_package.py
scripts/offline_algorithm_smoke.py
reports/phase_10_report.md
```

## Phase 11: Final Report

Goal: summarize completed implementation, skipped ROS2 build, and real ROS2 next commands.

Deliverable:

```text
reports/final_generation_report.md
```
