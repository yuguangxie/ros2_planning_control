# AGENTS.md — Low Speed AV Planning & Control ROS2 Generation Rules

This repository is intended to generate two downstream ROS2 modules for low-speed autonomous driving using the custom Low Speed Roadnet AD Package exported by the roadnet editor.

The final implementation MUST keep planning and control separated:

```text
src/
  low_speed_av_interfaces/   # msg/srv/action only
  low_speed_av_planning/     # roadnet loading, global planning, motion planning, speed planning
  low_speed_av_control/      # vehicle model, controller algorithms, command smoothing, safety output
  low_speed_av_bringup/      # launch, config, sample data, offline tools
```

## 1. Canonical AD Package Contract

Use the current roadnet editor ZIP schema as canonical. Do NOT use the older `manifest.json`, `trajectory/waypoints.json`, or root-level `validation_report.json` assumptions.

The loader MUST use:

```text
project_manifest.json
trajectory/waypoints.yaml
validation/validation_report.json
```

Canonical package file list:

```text
project_manifest.json
checksums.sha256
map/map_metadata.yaml
roadnet/roadnet.json
roadnet/topology.json
roadnet/route_graph.yaml
trajectory/waypoints.yaml
trajectory/waypoints.csv
trajectory/waypoint_index.json
semantics/areas.json
semantics/route_points.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
schemas/project_manifest.schema.json
schemas/roadnet.schema.json
schemas/topology.schema.json
schemas/waypoints.schema.json
schemas/waypoint_index.schema.json
schemas/semantics.schema.json
schemas/validation_report.schema.json
examples/mission.example.json
```

Manifest rules:

- Read `project_manifest.json` first.
- Support `schema == "low_speed_roadnet_ad_package"`.
- Support `schema_version == "1.1.0"` and compatible `1.1.x` patch versions.
- Read `coordinate_system.global_frame`, `coordinate_system.control_reference_frame`, and `units`.
- Reject packages where `validation.status == "failed"` or `validation.blocking_errors > 0`.
- Resolve files through `manifest.files` when available; use canonical fallback paths only when a key is absent.
- Verify `checksums.sha256` and/or `manifest.hashes` when present.

Waypoint mapping:

```text
waypoints.yaml field -> internal field
x                   -> x_m
y                   -> y_m
yaw                 -> yaw_rad
kappa               -> kappa_1pm
v_mps               -> target_speed_mps
s_m                 -> edge_s_m; regenerate route_s_m after edge concatenation
edge_id             -> edge_id
path_id             -> path_id
direction           -> direction / gear hint
flags               -> edge_start / edge_end annotations
```

`waypoint_index.json` may contain `end_index` from schema v1.1.0. Treat it as inclusive only if no `end_index_exclusive` exists. New code SHOULD support both forms:

```text
preferred: start_index + end_index_exclusive + count
legacy v1.1.0: start_index + end_index, interpreted as inclusive
```

## 2. System Architecture Rules

The implementation must follow the technical route:

```text
AD Package ZIP -> planning RoadnetLoader
               -> global planner A*/Dijkstra edge sequence
               -> motion planner reference trajectory + speed planner
               -> controller trajectory tracking
               -> Ackermann control command
```

Do not use Nav2 or Lanelet2 as the primary architecture. They may be referenced conceptually, but the project must consume the custom `roadnet/topology/waypoints` package.

Planning package responsibilities:

- Load and validate AD Package.
- Build directed graph from `roadnet/topology.json`.
- Run selectable global planner algorithm: `dijkstra` or `astar`.
- Convert edge sequence into a continuous reference trajectory using `trajectory/waypoints.yaml` and `trajectory/waypoint_index.json`.
- Apply speed planner: `constant`, `curvature`, or `obstacle_aware` stub.
- Publish `/planning/global_route`, `/planning/trajectory`, `/planning/status`.

Control package responsibilities:

- Subscribe configurable localization pose topic; default `/localization/pose`.
- Subscribe `/planning/trajectory`, `/vehicle/state`, `/safety/status` if available.
- Provide selectable controller algorithms: `pure_pursuit`, `stanley`, `lqr`, `mpc_sampler`.
- Support `front_ackermann` and `dual_ackermann` four-wheel Ackermann models.
- Apply command limit, command smoother, timeout handling, and controlled stop.
- Publish `/control/command` and `/control/status`.

## 3. ROS2 Topic and Config Requirements

All topic names MUST be configurable through YAML. Defaults:

```yaml
topics:
  localization_pose_topic: "/localization/pose"
  trajectory_topic: "/planning/trajectory"
  global_route_topic: "/planning/global_route"
  vehicle_state_topic: "/vehicle/state"
  safety_status_topic: "/safety/status"
  control_command_topic: "/control/command"
  planning_status_topic: "/planning/status"
  control_status_topic: "/control/status"
```

The localization input must accept at least `geometry_msgs/msg/PoseStamped`. Optionally support `PoseWithCovarianceStamped` via config.

## 4. Vehicle Model Requirements

Support four-wheel Ackermann chassis with two modes:

```yaml
vehicle:
  model: "front_ackermann"   # front_ackermann | dual_ackermann
```

Front Ackermann:

```text
kappa = tan(delta_front) / wheel_base
rear_steer = 0
```

Dual Ackermann counter-phase mode:

```text
kappa = (tan(delta_front) - tan(delta_rear)) / wheel_base
```

Use a configurable rear steering ratio for counter-phase steering:

```text
tan(delta_rear) = -rear_steer_ratio * tan(delta_front)
tan(delta_front) = kappa * wheel_base / (1 + rear_steer_ratio)
```

Limit front/rear steering angle, steering rate, speed, acceleration, deceleration, jerk if available.

## 5. Algorithm Plugin Pattern

A full pluginlib implementation is optional. The minimum acceptable design is an explicit registry/factory:

```text
GlobalPlannerFactory: dijkstra | astar
MotionPlannerFactory: reference_line | stop_and_wait | frenet_lite | hybrid_astar_parking
SpeedPlannerFactory: constant | curvature | obstacle_aware
ControllerFactory: pure_pursuit | stanley | lqr | mpc_sampler
VehicleModelFactory: front_ackermann | dual_ackermann
```

Each algorithm class must share a common interface and have deterministic unit-testable logic outside ROS2 nodes when possible.

## 6. No ROS2 Runtime in Codex Environment

The current Codex environment does not have ROS2. Do NOT fail the task because `colcon`, `ros2`, `rclcpp`, or generated interfaces cannot be built locally.

Instead:

- Generate ROS2 source files, CMakeLists, package.xml, msg/srv/action, launch, and configs correctly.
- Do not execute `colcon build` unless ROS2 is actually detected.
- Provide pure Python offline validation scripts.
- Run only commands available in the environment, such as:

```bash
python3 scripts/validate_expected_tree.py
python3 scripts/validate_sample_ad_package.py
python3 scripts/offline_algorithm_smoke.py
```

If a ROS2 command cannot run, write it to the phase report as `SKIPPED_ROS2_UNAVAILABLE`, not as a failure.

## 7. Phase Report Requirement

Every phase must create or update:

```text
reports/phase_xx_report.md
```

Each report must contain:

```text
# Phase XX Report
- Goal
- Files changed
- Key design decisions
- AD Package compatibility notes
- Config/topic compatibility notes
- Tests or offline checks run
- ROS2 commands skipped because ROS2 is unavailable
- Known limitations
- Next phase handoff
```

The final phase must create:

```text
reports/final_generation_report.md
```

## 8. Safety Rules

- If localization pose times out, control must command controlled stop.
- If trajectory times out, control must command controlled stop.
- If AD Package validation failed, planning must refuse to enter active planning.
- If no valid global route exists, planning must publish failure status and stop trajectory.
- Control output must be limited and smoothed before publishing.
- Never publish high speed by default. Default speed should be conservative, e.g. 0.3-0.8 m/s for demo.

## 9. Documentation Rules

Keep all Markdown documents in UTF-8. Use clear Chinese documentation. Use English identifiers in code and config. Avoid inventing message fields that are not documented in `docs/03_ros2_interfaces.md`.
