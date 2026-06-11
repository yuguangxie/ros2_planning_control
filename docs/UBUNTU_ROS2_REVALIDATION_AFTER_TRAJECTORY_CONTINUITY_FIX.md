# Ubuntu ROS2 轨迹连续性修复后复测步骤

## 目标

验证以下修复：

- Roadnet A `/plan_mission current_pose -> charging RP-017` 可成功，或失败原因明确证明为路网不可达。
- invalid task/parking/charging message 清晰，不被 current-pose matched 文案覆盖。
- `/planning/trajectory` 不再出现“前视段 + 远处目标段”的跳接。
- `/planning/full_reference_path` 可观察完整连续几何路线。
- `/planning/trajectory` 仍约 10 Hz。
- `/control/status` 仍约 5 Hz。
- SCU 27 deg clamp 不回退。
- Roadnet B `current_pose -> RP-003` 仍成功。
- 旧 `/plan_route` 接口仍兼容。

## 构建

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

人工确认：

- `low_speed_av_interfaces` 构建通过。
- `low_speed_av_planning` 构建通过。
- `low_speed_av_control` 构建通过。
- `low_speed_av_simulation` 构建通过。
- `low_speed_av_bringup` 构建通过。
- `colcon test-result` 至少包含 `offline_trajectory_continuity` 测试。

## Roadnet A charging 复测

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1

ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

另开终端：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET
```

调用 mission：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

观察：

```bash
ros2 topic echo --once /planning/global_route
ros2 topic echo --once /planning/full_reference_path
ros2 topic echo --once /planning/trajectory
ros2 topic echo --once /planning/status
ros2 topic echo --once /simulation/trajectory_path
```

期望：

- service `success=true`，或失败 message 明确说明不可达原因。
- 不应再出现 charging point 找不到、无效 null node、或 current-pose 文案覆盖 goal 错误的问题。
- `/planning/full_reference_path` 包含完整几何路径，终点接近 `RP-017`。
- `/planning/trajectory` 是从当前车辆附近开始的连续局部轨迹，不直接跳到远处 charging edge。

## Roadnet A trajectory 连续性复测

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-001', goal_parking_point_id: ''}"

ros2 topic echo --once /planning/full_reference_path
ros2 topic echo --once /planning/trajectory
ros2 topic hz /planning/trajectory
```

期望：

- `/planning/full_reference_path` 是完整路线。
- `/planning/trajectory` 是连续局部段。
- `/planning/trajectory` 约 10 Hz。

## Roadnet B route_N0001_N0001 回归

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_2

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-003', goal_parking_point_id: ''}"
```

期望：

- 不应因 `route_N0001_N0001` 返回空 trajectory。
- 未到达时生成非空 trajectory。
- 已到达时返回 success + stop/arrived trajectory。

## 错误文案复测

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'BAD_CHARGING'}"
```

期望 message 包含：

```text
charging point not found: BAD_CHARGING
```

并允许附加：

```text
start: matched current pose ...
```

## SCU clamp 回归

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0015', goal_node_id: 'N0014', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"

ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- `abs(scu_steering_angle_front) <= 27.0`
- `abs(scu_steering_angle_rear) <= 27.0`
- speed 非负
- reverse shift = 3

参数确认：

```bash
ros2 param get /low_speed_av_control scu.max_steering_angle_deg
ros2 param get /low_speed_av_control scu.overrange_policy
```

## Control status 回归

```bash
ros2 topic echo /control/status
ros2 topic hz /control/status
```

期望：

- active/tracking 期间可以持续捕获状态样本。
- 频率约 5 Hz。

## Windows Codex 说明

本文档由 Windows Codex 环境生成，未在本机执行 ROS2 命令：

```text
SKIPPED_ROS2_UNAVAILABLE
```
