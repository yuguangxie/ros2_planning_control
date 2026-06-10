# Ubuntu ROS2 Revalidation After Fix

## 目标

本文件用于在 Ubuntu + ROS2 Humble 环境中复测本轮运行期修复：

- task point `null` / fallback 修复
- planning trajectory 周期重发
- `/planning/roadnet_status` 晚订阅可观测性
- parking point fixture 说明
- SCU 安全输出回归

Windows Codex 环境未执行 ROS2 命令，相关命令在此作为 Ubuntu 人工复测步骤。

## 1. 构建与测试

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

期望：

- 7 个包构建通过。
- 不出现 ROS1/catkin/roscpp 依赖错误。
- `low_speed_av_planning`、`low_speed_av_simulation`、`low_speed_av_control` 均可构建。

## 2. 启动仿真定位与可视化

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

检查：

```bash
ros2 topic echo /localization/pose --once
ros2 topic hz /localization/pose
ros2 topic echo /simulation/roadnet_markers --once
```

期望：

- `/localization/pose` 持续发布。
- frame 为 `map`。
- roadnet markers 非空。

## 3. 启动规划控制

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z
```

检查：

```bash
ros2 node list
ros2 service list
ros2 topic list
```

## 4. Roadnet Status 晚订阅复测

直接晚订阅：

```bash
ros2 topic echo /planning/roadnet_status --once
```

如果命令等待，可连续观察：

```bash
ros2 topic echo /planning/roadnet_status
```

期望：

- 能看到 `ready: true`。
- late subscriber 能通过 transient local 或周期重发看到最近状态。

## 5. Task Point 目标复测

当前定位作为起点，任务点作为目标：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-001', goal_parking_point_id: ''}"
```

期望：

- `success=True`
- 不再出现 `"null"` 或 `start or goal node is not in topology`
- route 目标应解析到 `RP-001` 所在 edge 的终点，当前包中为 `N0008`
- `/planning/trajectory` 非空
- RViz 中 route/trajectory 更新

## 6. Task Point 起点到 Task Point 目标复测

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: 'RP-003', goal_task_point_id: 'RP-001', goal_parking_point_id: ''}"
```

期望：

- `success=True`
- start task point 使用 node-level fallback：`linked_edge_id` 的 `from_node_id`
- goal task point 使用 node-level fallback：`linked_edge_id` 的 `to_node_id`
- 该行为不是精确 edge projection，后续如需任务点精确起终点，应增加 edge projection 与轨迹裁剪。

## 7. 无效 Task Point 复测

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'BAD_TASK', goal_parking_point_id: ''}"
```

期望：

- `success=False`
- message 类似：

```text
task point not found: BAD_TASK
```

- `/planning/trajectory` 为 `failure_stop`
- 控制输出安全停车

## 8. Parking Point 说明与失败路径复测

当前正式包：

```text
roadnet_ad_package_20260610T012525Z/semantics/parking_points.json
parking_points: []
```

因此正式包无法验证真实 parking point 成功路径。可以验证无效 parking point 安全失败：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: '', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: 'BAD_PARKING'}"
```

期望：

- `success=False`
- 发布安全停车轨迹

parking point 成功路径需要一个包含真实 parking point 的 AD Package，或使用离线 fixture 脚本验证。

## 9. Trajectory 持续性复测

先触发一条 node-id 路线：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0005', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

观察轨迹持续发布：

```bash
ros2 topic hz /planning/trajectory
```

默认期望：

```text
about 10 Hz
```

观察 SCU 和控制状态：

```bash
ros2 topic hz /yunle_chassis/control/scu_control_command
ros2 topic echo /control/status
```

期望：

- `/planning/trajectory` 持续发布。
- control 不应在 0.5 秒后仅因为 trajectory 单次发布而进入 `trajectory_timeout`。
- SCU 可持续输出合理命令，或在安全/到达条件下停车。

## 10. Safety 回归

触发安全状态：

```bash
ros2 topic pub --once /safety/status low_speed_av_interfaces/msg/ModuleStatus \
  "{module_name: 'manual_test', state: 'estop', level: 2, message: 'manual estop'}"
```

观察：

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command --once
```

期望：

- `scu_brake_enable=true`
- `scu_target_speed=0`
- `scu_steering_angle_front=0`
- `scu_steering_angle_rear=0`
- shift 仍为合法值，默认 D=1

## 11. Roadnet 曲率 Warning 安全确认

当前包仍有非阻塞 warning：

- `HIGH_CURVATURE`
- `CURVATURE_CONTINUITY`
- `WAYPOINT_CURVATURE_EXCEEDS_CONSTRAINT`

实车前请确认：

- 使用低速策略。
- 默认 speed planner 是否为 `curvature`。
- SCU 输出速度满足低速测试要求。
- 必要时重新平滑并导出路网。

