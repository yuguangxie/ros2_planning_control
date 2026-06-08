# ROS2 命令示例

本文给出可复制的 ROS2 命令模板。当前 Windows Codex 环境未执行这些 ROS2 命令；请在真实 ROS2 环境中运行。

## 1. 构建命令

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

## 2. 启动命令

完整 bringup：

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
```

覆盖 AD Package：

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/ad_package
```

单独启动 planning：

```bash
ros2 launch low_speed_av_planning planning.launch.py
```

单独启动 control：

```bash
ros2 launch low_speed_av_control control.launch.py
```

## 3. 系统确认

```bash
ros2 node list
ros2 topic list
ros2 service list
```

期望节点：

```text
/low_speed_av_planning
/low_speed_av_control
```

## 4. 参数查看

```bash
ros2 param get /low_speed_av_planning roadnet.package_path
ros2 param get /low_speed_av_planning global_planner.algorithm
ros2 param get /low_speed_av_planning motion_planner.algorithm
ros2 param get /low_speed_av_planning speed_planner.algorithm
ros2 param get /low_speed_av_planning topics.trajectory_topic

ros2 param get /low_speed_av_control output.mode
ros2 param get /low_speed_av_control topics.localization_pose_topic
ros2 param get /low_speed_av_control topics.scu_command_topic
ros2 param get /low_speed_av_control controller.algorithm
ros2 param get /low_speed_av_control vehicle.model
ros2 param get /low_speed_av_control safety.estop_latched
```

## 5. 查看接口

```bash
ros2 interface show low_speed_av_interfaces/srv/ReloadRoadnet
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/srv/SetPlannerAlgorithm
ros2 interface show low_speed_av_interfaces/srv/SetControllerAlgorithm
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
ros2 interface show chassis_interfaces/msg/ScuControlCommand
```

## 6. 重载 AD Package

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/ad_package'}"
```

查看状态：

```bash
ros2 topic echo /planning/roadnet_status --once
ros2 topic echo /planning/status --once
```

## 7. 触发路线规划

节点 ID 示例：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

任务点目标示例：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'T001', goal_parking_point_id: ''}"
```

停车点目标示例：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: '', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: 'P001'}"
```

## 8. 切换规划算法

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"
```

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'dijkstra', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'constant'}"
```

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'stop_and_wait', speed_planner_algorithm: 'constant'}"
```

## 9. 观察规划输出

```bash
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /planning/status
```

## 10. 发布示例定位

控制节点需要持续定位。示例：

```bash
ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped \
  "{header: {frame_id: 'map'}, pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}}}" \
  --rate 20
```

## 11. 发布示例车辆状态

```bash
ros2 topic pub /vehicle/state low_speed_av_interfaces/msg/VehicleState \
  "{header: {frame_id: 'base_link'}, speed_mps: 0.0, acceleration_mps2: 0.0, steering_angle_rad: 0.0, front_steering_angle_rad: 0.0, rear_steering_angle_rad: 0.0, gear: 1, autonomous_enabled: true, brake_pressed: false, fault_code: ''}" \
  --rate 20
```

## 12. 切换控制器和车辆模型

LQR + 前轮 Ackermann：

```bash
ros2 service call /low_speed_av_control/set_controller_algorithm \
  low_speed_av_interfaces/srv/SetControllerAlgorithm \
  "{controller_algorithm: 'lqr', vehicle_model: 'front_ackermann'}"
```

Pure Pursuit + 双 Ackermann：

```bash
ros2 service call /low_speed_av_control/set_controller_algorithm \
  low_speed_av_interfaces/srv/SetControllerAlgorithm \
  "{controller_algorithm: 'pure_pursuit', vehicle_model: 'dual_ackermann'}"
```

Stanley：

```bash
ros2 service call /low_speed_av_control/set_controller_algorithm \
  low_speed_av_interfaces/srv/SetControllerAlgorithm \
  "{controller_algorithm: 'stanley', vehicle_model: ''}"
```

MPC sampler：

```bash
ros2 service call /low_speed_av_control/set_controller_algorithm \
  low_speed_av_interfaces/srv/SetControllerAlgorithm \
  "{controller_algorithm: 'mpc_sampler', vehicle_model: ''}"
```

## 13. 观察控制输出

最终 Yunle SCU 输出：

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/control/scu_control_command
```

内部 debug 输出仅在 `output.mode=internal` 或 `both` 时发布：

```bash
ros2 topic echo /control/command
```

控制状态：

```bash
ros2 topic echo /control/status
```

## 14. 发布安全 Estop

触发：

```bash
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus \
  "{module_name: 'safety', state: 'estop', level: 2, message: 'manual estop test'}" \
  --once
```

期望 SCU：

- `scu_brake_enable: true`
- `scu_target_speed: 0.0`
- 前/后 steering 为 `0.0`
- `scu_shift_level_request` 为合法值 1/2/3

## 15. 清除 Estop

当前默认 `safety.estop_latched=true`，清除条件由 `safety.clear_level` 和 `safety.clear_state` 控制。默认 clear state 为 `ok`，clear level 为 `0`。

```bash
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus \
  "{module_name: 'safety', state: 'ok', level: 0, message: 'manual clear'}" \
  --once
```

清除后仍需要有效定位和有效轨迹，控制才会恢复正常输出。

## 16. D/R/Stop 观察

前进 D：

1. 规划普通 forward route。
2. 观察 `/yunle_chassis/control/scu_control_command`。
3. 确认 `scu_shift_level_request=1`，`scu_target_speed>=0`。

倒车 R：

1. 使用包含 reverse gear/trajectory 的 AD Package 或测试轨迹。
2. 观察 SCU。
3. 确认 `scu_shift_level_request=3`，速度仍为非负 km/h。

停车：

1. 触发 estop、停止定位、停止轨迹或发布空轨迹。
2. 观察 SCU。
3. 确认 brake true、speed 0、steering 0。

## 17. ROS2 不可用记录

当前 Windows Codex 环境没有执行上述 ROS2 命令。可先执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check_ros2_env.ps1
```

若输出 `SKIPPED_ROS2_UNAVAILABLE`，说明只能进行离线脚本验证，不能声明 ROS2 build/launch/topic/service 已通过。
