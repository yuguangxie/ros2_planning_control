# 语义目标点修复后的 Ubuntu ROS2 复测流程

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

- `low_speed_av_interfaces` 重新生成 `PlanMission.srv`。
- planning/control/simulation/bringup 均构建通过。
- 无 catkin/roscpp 依赖错误。

## 启动仿真

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

可选：将模拟定位放到指定语义点或 edge 位置：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  initial_task_point_id:=RP-003 \
  rviz:=true
```

## 启动 planning/control

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z
```

## 复测 RP-003

兼容旧服务：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-003', goal_parking_point_id: ''}"
```

新增业务服务：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-003'}"
```

观察：

```bash
ros2 topic echo --once /planning/global_route
ros2 topic echo --once /planning/trajectory
ros2 topic echo --once /planning/status
ros2 topic echo --once /simulation/trajectory_path
```

期望：

- 不再因为 `route_N0001_N0001` 返回 `motion planner produced empty trajectory`。
- 未到达 RP-003 时，`/planning/trajectory` 非空。
- 已到达 RP-003 时，返回 success + `arrived_stop`。
- trajectory 末端接近 RP-003 的 `x/y/yaw`。
- `/planning/trajectory` 仍约 10 Hz 重发。

## 节点规划兼容复测

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0005', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

期望：node-id 规划仍 `success=true`。

## SCU 转角复测

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
- reverse shift 为 3

## control status 复测

```bash
ros2 topic echo /control/status
```

期望：active/tracking 期间能持续捕获 `tracking` 状态样本。

## 失败路径复测

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'BAD_TASK', goal_parking_point_id: ''}"
```

期望：

- service `success=false`
- 错误信息清晰
- 发布 `failure_stop`
- SCU 输出 brake stop

## Windows Codex 说明

Windows Codex 环境未执行 `colcon build`、`ros2 launch`、`ros2 topic` 或 `ros2 service`。这些命令属于：

```text
SKIPPED_ROS2_UNAVAILABLE
```
