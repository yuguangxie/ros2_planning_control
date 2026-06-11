# Ubuntu ROS2 倒车策略与当前定位起点复测步骤

## 构建

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

期望：

- `low_speed_av_planning`、`low_speed_av_control`、`low_speed_av_simulation`、`low_speed_av_bringup` 构建通过。
- 无 ROS1 `catkin/roscpp` 依赖错误。

## 启动 Roadnet A

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1

ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

另一个终端：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET
```

## 检查参数

```bash
ros2 param get /low_speed_av_planning planning.reverse.allow_reverse_planning
ros2 param get /low_speed_av_planning planning.reverse.allow_reverse_local_segment
ros2 param get /low_speed_av_planning planning.start_anchor.include_current_edge_prefix
ros2 param get /low_speed_av_planning planning.start_anchor.max_first_trajectory_point_distance_m
```

默认期望：

```text
planning.reverse.allow_reverse_planning: false
planning.reverse.allow_reverse_local_segment: false
planning.start_anchor.include_current_edge_prefix: true
planning.start_anchor.max_first_trajectory_point_distance_m: 2.0
```

## current pose -> RP-001

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"

ros2 topic echo --once /planning/full_reference_path
ros2 topic echo --once /planning/trajectory
ros2 topic echo --once /planning/status
```

期望：

- service `success=true`。
- message/status 包含 current pose 匹配到 waypoint、edge、`s_on_edge`，并说明 start anchor 没有在几何上退化为节点。
- `/planning/full_reference_path` 开头包含 `E_C-012_F` 剩余段。
- `/planning/trajectory` 第一轨迹点接近当前定位/projection，距离小于 `2.0 m`。
- 不再从 `E_C-002_F` 直接开始。

## reverse disabled 下 current pose -> RP-008

```bash
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_planning false
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_local_segment false

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-008', goal_parking_point_id: ''}"

ros2 topic echo --once /planning/trajectory
ros2 topic echo --once /planning/status
ros2 topic echo --once /yunle_chassis/control/scu_control_command
```

期望：

- 不出现 reverse gear。
- 不出现 `semantic_reverse_local`。
- 不出现 SCU `shift=3`。
- 如果 forward detour 可达，message/status 包含 `reverse disabled; using forward detour`。
- 如果 forward detour 不可达，service `success=false`，message/status 包含 `reverse planning is disabled`，并发布 `failure_stop`。

## reverse enabled 下 current pose -> RP-008

```bash
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_planning true
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_local_segment true

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-008', goal_parking_point_id: ''}"

ros2 topic echo --once /planning/trajectory
ros2 topic echo --once /planning/status
ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- 可以出现 reverse gear。
- message/status 包含 `reverse local segment selected`。
- SCU 可映射为 `shift=3`，速度仍为非负 km/h，转角仍受 27 deg clamp 约束。

## 回归验证

Roadnet A charging：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

Roadnet A parking：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'parking', goal_id: 'RP-015'}"
```

Roadnet B `route_N0001_N0001` 回归：

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_2

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-003', goal_parking_point_id: ''}"
```

SCU clamp 回归：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0015', goal_node_id: 'N0014', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"

ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- Roadnet A `RP-017` 不回退。
- Roadnet A `RP-015` 不回退。
- Roadnet B `RP-003` 不回退。
- `/planning/trajectory` 仍约 10 Hz。
- `/control/status` 仍约 5 Hz。
- SCU steering 不超过 27 deg。

