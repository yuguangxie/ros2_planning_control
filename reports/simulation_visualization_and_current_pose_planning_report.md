# Simulation Visualization and Current-Pose Planning Report

## Goal

新增 ROS2 仿真可视化模块，并改进规划节点，使 `PlanRoute` 在起点字段为空时可以使用当前 `/localization/pose` 作为路线起点。

## Files changed

- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_simulation/package.xml`
- `src/low_speed_av_simulation/CMakeLists.txt`
- `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp`
- `src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp`
- `src/low_speed_av_simulation/config/simulation_params.yaml`
- `src/low_speed_av_simulation/launch/simulation_visualization.launch.py`
- `src/low_speed_av_simulation/rviz/roadnet_simulation.rviz`
- `scripts/offline_simulation_smoke.py`
- `scripts/validate_expected_tree.py`
- `docs/SIMULATION_VISUALIZATION_USAGE.md`
- `docs/LOCALIZATION_POSE_SIMULATION_GUIDE.md`
- `docs/PLANNING_CURRENT_POSE_START_ADJUSTMENT.md`
- `docs/SIMULATION_AND_PLANNING_INTEGRATION_FLOW.md`

## Packages added

- `low_speed_av_simulation`

## New topics

```text
/simulation/roadnet_markers
visualization_msgs/msg/MarkerArray

/simulation/route_markers
visualization_msgs/msg/MarkerArray

/simulation/trajectory_path
nav_msgs/msg/Path

/simulation/vehicle_markers
visualization_msgs/msg/MarkerArray
```

Existing topics preserved:

```text
/localization/pose
/planning/global_route
/planning/trajectory
/planning/status
/planning/roadnet_status
/yunle_chassis/control/scu_control_command
```

## New services

```text
/simulation/start
std_srvs/srv/Trigger

/simulation/pause
std_srvs/srv/Trigger

/simulation/reset
std_srvs/srv/Trigger
```

Existing planning services preserved:

```text
/low_speed_av_planning/reload_roadnet
/low_speed_av_planning/plan_route
/low_speed_av_planning/set_planner_algorithm
```

## New parameters

Planning:

```yaml
planning:
  localization_timeout_s: 1.0
  use_current_pose_as_start: true
  start_match_max_distance_m: 3.0
  start_match_max_heading_error_rad: 1.57
  start_match_prefer_edge_projection: true
```

Simulation:

```yaml
publish_rate_hz: 20.0
frame_id: "map"
mode: "fixed_pose"
initial_x: 0.554
initial_y: 1.473
initial_yaw: -0.9178
speed_mps: 0.5
loop: true
start_paused: false
trajectory_topic: "/planning/trajectory"
pose_topic: "/localization/pose"
```

## Design decisions

- 未修改 `PlanRoute.srv`，保持显式 `start_node_id` 调用兼容。
- 当 start 字段为空时，规划节点才使用当前定位推断起点。
- 匹配器使用 waypoint/edge 最近邻逻辑，并检查距离、航向和定位新鲜度。
- RViz 可视化使用标准 `visualization_msgs/msg/MarkerArray` 和 `nav_msgs/msg/Path`。
- 模拟定位节点命名中包含 `sim`，避免与真实定位混淆。
- 控制模块输出保持不变，最终底盘 topic 仍为 `/yunle_chassis/control/scu_control_command`。

## Tests run

```powershell
uv run python scripts\validate_expected_tree.py
```

Result:

```text
Expected tree OK: .
```

```powershell
uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
```

Result:

```text
AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)
```

```powershell
uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z
```

Result:

```text
Offline simulation smoke OK: nodes=16, edges=22, waypoints=496, areas=4, task_points=7, matched_start={'start_node': 'N0001', 'waypoint_id': 'WP_E_C-001_F_000000', 'edge_id': 'E_C-001_F', 'distance_m': 0.0, 'heading_error_rad': 0.0}, route=['E_C-001_F', 'E_L-001_F'], trajectory_points=30, expected_failures=['current localization pose is not available', 'current localization pose is stale']
```

```powershell
uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package
```

Result:

```text
Offline algorithm smoke OK: route=['E_L001_F', 'E_L002_F'], traj_points=6, pp=(0.500,0.000), stanley=(0.500,0.000), ackermann=finite, estop=ok
```

YAML parsing:

```powershell
uv run --with pyyaml python -c "import yaml, pathlib; [yaml.safe_load(pathlib.Path(p).read_text(encoding='utf-8')) for p in ['src/low_speed_av_planning/config/planning_params.yaml','src/low_speed_av_bringup/config/planning_params.yaml','src/low_speed_av_simulation/config/simulation_params.yaml']]; print('YAML OK')"
```

Result:

```text
YAML OK
```

## SKIPPED_ROS2_UNAVAILABLE

Current Codex Windows environment did not provide `colcon` or `ros2`. These commands were not run and are not claimed as passed:

```bash
colcon build --symlink-install
colcon test
ros2 launch low_speed_av_simulation simulation_visualization.launch.py ...
ros2 topic echo /simulation/roadnet_markers
ros2 topic echo /localization/pose
ros2 service call /low_speed_av_planning/plan_route ...
```

## Remaining risks

- C++ compilation still needs validation in Ubuntu/ROS2 because this environment lacks ROS2 headers and generated interfaces.
- Current-pose matcher is waypoint/edge based, not a continuous projection planner.
- If pose is on the latter half of an edge, planner may start at the edge to-node and skip the already-traveled part by design.
- Simulated localization must not be connected to a live vehicle unless chassis output is disabled or environment is safe.
- Existing roadnet has validation warnings for high curvature; use low speed for manual validation.

## Recommended manual validation

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  launch_planning_control:=true \
  rviz:=true
```

Then run:

```bash
ros2 topic echo /localization/pose
ros2 topic echo /planning/roadnet_status
ros2 topic echo /simulation/roadnet_markers
ros2 service list
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /yunle_chassis/control/scu_control_command
```
