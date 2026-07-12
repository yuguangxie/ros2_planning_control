# Yunle SCU 底盘控制输出说明

## 目标
控制模块最终面向 Yunle ROS2 底盘驱动发布：

```text
topic: /yunle_chassis/control/scu_control_command
type:  chassis_interfaces/msg/ScuControlCommand
```

工程仍可保留内部 `/control/command` 调试输出，但底盘侧必须使用 SCU topic。

## ROS2 依赖
`low_speed_av_control` 需要：

```cmake
find_package(rclcpp REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(chassis_interfaces REQUIRED)
```

C++ include：

```cpp
#include "chassis_interfaces/msg/scu_control_command.hpp"
```

## 配置
```yaml
output:
  mode: "both"   # internal | scu_control_command | both

topics:
  scu_command_topic: "/yunle_chassis/control/scu_control_command"

scu:
  max_steering_angle_deg: 30.0
  max_target_speed_kmh: 5.0
  front_steer_sign: 1.0
  rear_steer_sign: 1.0
  stop_shift_level: 1
  torque_or_speed_mode: 1
  steering_angle_speed_valid: false
  brake_force_command_valid: false
  lights:
    left: 0
    right: 0
    position: 0
    low_beam: 0
```

## 字段映射
| 内部字段 | SCU 字段 | 规则 |
|---|---|---|
| `gear=drive` | `scu_shift_level_request=1` | D 挡 |
| `gear=neutral` | `scu_shift_level_request=2` | N 挡 |
| `gear=reverse` | `scu_shift_level_request=3` | R 挡 |
| unknown gear | stop command | 不发布非法 shift |
| `speed_mps` | `scu_target_speed` | `abs(speed_mps) * 3.6`，单位 km/h |
| `front_steering_angle_rad` | `scu_steering_angle_front` | rad 转 deg，乘 `front_steer_sign` |
| `rear_steering_angle_rad` | `scu_steering_angle_rear` | rad 转 deg，乘 `rear_steer_sign` |
| `brake > 0` 或 `emergency_stop` | `scu_brake_enable=true` | 发布停车命令 |

## 合法性保护
- `scu_shift_level_request` 只允许 1、2、3。
- 内部未知挡位会映射为刹车停车命令，使用 `scu.stop_shift_level`，默认 1。
- 转角非有限或超出 `[-max_steering_angle_deg, +max_steering_angle_deg]` 时，该转角信号置 0 并输出 warning。
- 速度非有限或超过 `max_target_speed_kmh` 时，速度置 0 并输出 warning。
- 速度永不为负，倒车方向只由 R 挡选择。

## 停车命令
Planning failure/emergency trajectory、车辆门控、定位/轨迹/VehicleState 超时、空轨迹、无效命令和 NaN/Inf guard 均发布：

```text
scu_shift_level_request = stop_shift_level, default 1
scu_steering_angle_front = 0.0
scu_steering_angle_rear = 0.0
scu_target_speed = 0.0
scu_brake_enable = true
scu_torque_or_speed_mode = 1
steering_angle_speed_valid = false
brake_force_command_valid = false
lights = 0
```

Control 先生成带 reason 的 `/control/command`，再映射 SCU 停车。controlled stop 的 `emergency_stop=false`，hard estop 的 `emergency_stop=true`；当前 SCU 接口只有 `scu_brake_enable` 布尔量，因此两者在 SCU 层都表现为 brake true、target speed 0，无法表达不同制动力曲线。

Control watchdog 只保证 Control 存活时对上游输入 fail closed，并以默认 50 Hz 同时发布 internal/SCU 输出。100 ms deadline 是软件告警阈值，不是允许的正常 jitter；状态中记录 max/p95 interval、missed cycles 和 publish age。

项目方声明底盘连续 500 ms 未收到 CAN 0x121 时硬件停车。该机制只处理 0x121 完全消失，不能替代 semantic stop，也不能证明制动力、停车距离和恢复行为。本阶段未修改 Yunle Chassis Driver；供应商/bench/HIL 证据缺失，状态为 `DECLARED_NOT_HIL_VERIFIED`，不得写成 HIL PASS。

## ROS2 验证命令
以下命令必须在真实 ROS2 环境运行。本 Codex Windows 环境若没有 ROS2，应记录为 `SKIPPED_ROS2_UNAVAILABLE`。

```bash
colcon build --packages-select low_speed_av_interfaces low_speed_av_control
colcon test --packages-select low_speed_av_control
ros2 launch low_speed_av_control control.launch.py
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus "{module_name: 'safety', state: 'estop', level: 2, message: 'test estop'}"
```
