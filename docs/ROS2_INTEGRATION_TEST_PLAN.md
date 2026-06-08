# ROS2 集成测试计划

## 目标
在真实 ROS2 环境中验证低速自动驾驶规划/控制工程的 build、test、launch、service、topic、Yunle SCU 底盘输出和安全停车链路。

当前 Windows Codex 环境可能没有 ROS2。无 ROS2 时只能记录 `SKIPPED_ROS2_UNAVAILABLE`，不能声称 `colcon build`、`colcon test`、`ros2 launch` 或 topic/service 验证成功。

## 环境检查
先运行：

```powershell
.\scripts\check_ros2_env.ps1
```

如果输出 `SKIPPED_ROS2_UNAVAILABLE`，停止 ROS2 集成测试，只运行离线 Python 检查。

## 构建与测试
在已经 source ROS2 和当前 workspace 的环境中执行：

```bash
colcon build
colcon test
colcon test-result --verbose
```

## 启动 demo
```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
```

## 规划链路验证
```bash
ros2 service call /low_speed_av_planning/plan_route low_speed_av_interfaces/srv/PlanRoute "{start_node_id: 'N0001', goal_node_id: 'N0003'}"
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /planning/status
```

## 控制链路验证
默认控制输出为 Yunle SCU topic：

```bash
ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped "{header: {frame_id: 'map'}, pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}"
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic echo /control/status
```

如果 `output.mode` 配置为 `both` 或 `internal`，还可以检查内部调试命令：

```bash
ros2 topic echo /control/command
```

## 安全急停验证
```bash
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus "{module_name: 'safety', state: 'estop', level: 2, message: 'test estop'}"
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus "{module_name: 'safety', state: 'ok', level: 0, message: 'clear'}"
```

预期：急停时 `scu_brake_enable=true`，`scu_target_speed=0`，前后转角为 0，`scu_shift_level_request` 为 1/2/3 中的合法值。

## 验收点
- `PlanRoute N0001 -> N0003` 返回 success，并发布非空 `/planning/trajectory`。
- 无效目标或 no-go 阻塞时发布 failure status 和安全停车轨迹。
- 有效 pose 与 trajectory 下，`/yunle_chassis/control/scu_control_command` 数值有限。
- `scu_shift_level_request` 永远是 1、2、3。
- `scu_target_speed` 单位为 km/h，且非负。
- `scu_steering_angle_front/rear` 单位为 degree。
- estop 激活时发布刹车停车命令。
- `colcon` 和 `ros2` 命令只有实际执行成功时才可写入成功报告。
