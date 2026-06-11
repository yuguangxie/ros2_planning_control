# SCU Control Command 字段映射审计

## Objective

审计 `low_speed_av_control` 输出的 `chassis_interfaces/msg/ScuControlCommand` 是否覆盖 `yunle_chassis/chassis_driver` 编码 CAN `SCU_Control_Command` 所需字段，并确认字段单位、合法值和安全默认值是否一致。

## Scope

- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg`
- `src/low_speed_av_control/src/scu_command_mapper.cpp`
- `src/low_speed_av_control/src/control_node.cpp`
- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp`
- `src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp`
- `src/yunle_chassis/Yunle_CAN_release.dbc`

## Status: Pass

静态代码审计结论：字段集合、挡位、速度单位、转角单位和安全停车字段与 driver 编码逻辑一致。ROS2 运行期 topic 连接未在 Windows 环境验证，记为 `SKIPPED_ROS2_UNAVAILABLE`。

## Evidence

- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:3` 定义 `SHIFT_LEVEL_D=1`。
- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:4` 定义 `SHIFT_LEVEL_N=2`。
- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:5` 定义 `SHIFT_LEVEL_R=3`。
- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:11` 到 `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:19` 说明前/后轮转角单位为 degree。
- `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:21` 到 `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:25` 说明 `scu_target_speed` 为 km/h 速度幅值，方向由挡位决定。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:11` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:13` 定义 D/N/R 为 1/2/3。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:51` 将 rad 转 deg 并乘转角符号。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:73` 将 m/s 转为 `abs(mps) * 3.6` km/h。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:89` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:100` 将内部 gear 映射到 SCU shift。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:115` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:131` 生成安全停车 SCU 命令。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:153` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:176` 生成正常 SCU 命令。
- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:44` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:50` 对非法 shift 直接丢弃整帧。
- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:52` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:75` 对速度和转角做有限值/范围检查，异常值按 0 下发。
- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:82` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:98` 将 ROS 字段编码到 CAN `SCU_Control_Command`。
- `src/yunle_chassis/Yunle_CAN_release.dbc:124` 到 `src/yunle_chassis/Yunle_CAN_release.dbc:137` 给出 0x121 CAN 信号定义。

## 字段清单

| 字段 | 类型 | 单位/合法值 | 默认语义 |
|---|---|---|---|
| `SHIFT_LEVEL_D` | `uint8` constant | `1` | D 挡 |
| `SHIFT_LEVEL_N` | `uint8` constant | `2` | N 挡 |
| `SHIFT_LEVEL_R` | `uint8` constant | `3` | R 挡 |
| `scu_shift_level_request` | `uint8` | 只允许 1/2/3 | 挡位请求 |
| `scu_steering_angle_front` | `float32` | deg，物理前轮转角 | 前轮转角命令 |
| `scu_steering_angle_rear` | `float32` | deg，物理后轮转角 | 后轮转角命令 |
| `scu_target_speed` | `float32` | km/h，非负速度幅值 | 目标速度 |
| `scu_brake_enable` | `bool` | true/false | 制动使能 |
| `gw_left_turn_light_request` | `uint8` | 0..3 | 左转向灯 |
| `gw_right_turn_light_request` | `uint8` | 0..3 | 右转向灯 |
| `gw_position_light_request` | `uint8` | 0..3 | 位置灯 |
| `gw_low_beam_request` | `uint8` | 0..3 | 近光灯 |
| `scu_torque_or_speed_mode` | `uint8` | 0/1 | 默认速度模式 1 |
| `steering_angle_speed_valid` | `bool` | true/false | 默认 false |
| `brake_force_command_valid` | `bool` | true/false | 默认 false |

## 字段映射表

| ScuControlCommand 字段 | control 是否赋值 | control 来源 | 单位 | driver 是否使用 | driver 用途 | 是否兼容 |
|---|---|---|---|---|---|---|
| `scu_shift_level_request` | 是 | `gear_to_shift()`，内部 gear 1/2/4 到 D/R/N | enum | 是 | `SCU_Shift_Level_Request` bit 0/2 | 兼容 |
| `scu_steering_angle_front` | 是 | `front_steering_angle_rad` -> deg -> sign -> clamp | deg | 是 | `SCU_Steering_Angle_Front` bit 8/8 | 兼容 |
| `scu_steering_angle_rear` | 是 | `rear_steering_angle_rad` -> deg -> sign -> clamp | deg | 是 | `SCU_Steering_Angle_Rear` bit 16/8 | 兼容 |
| `scu_target_speed` | 是 | `abs(speed_mps) * 3.6` | km/h | 是 | `SCU_Target_Speed` bit 24/9，scale 0.1 | 兼容 |
| `scu_brake_enable` | 是 | stop/brake/emergency 为 true，正常为 false | bool | 是 | `SCU_Brake_Enable` bit 33/1 | 兼容 |
| `gw_left_turn_light_request` | 是 | `scu.lights.left`；stop 强制 0 | raw | 是 | `GW_Left_Turn_Light_Request` bit 40/2 | 兼容 |
| `gw_right_turn_light_request` | 是 | `scu.lights.right`；stop 强制 0 | raw | 是 | `GW_Right_Turn_Light_Request` bit 42/2 | 兼容 |
| `gw_position_light_request` | 是 | `scu.lights.position`；stop 强制 0 | raw | 是 | `GW_Position_Light_Request` bit 46/2 | 兼容 |
| `gw_low_beam_request` | 是 | `scu.lights.low_beam`；stop 强制 0 | raw | 是 | `GW_Low_Beam_Request` bit 48/2 | 兼容 |
| `scu_torque_or_speed_mode` | 是 | `scu.torque_or_speed_mode`，默认 1 | raw | 是 | `SCU_Torque_Or_Speed_Mode` bit 58/1 | 兼容 |
| `steering_angle_speed_valid` | 是 | `scu.steering_angle_speed_valid`，默认 false；stop false | bool | 是 | `Steering_Angle_Speed_Valid` bit 60/1 | 兼容 |
| `brake_force_command_valid` | 是 | `scu.brake_force_command_valid`，默认 false；stop false | bool | 是 | `Brake_Force_Command_Valid` bit 61/1 | 兼容 |
| `scu_drive_mode_request` | 不存在 | driver 内部固定 | raw | 是，内部固定 | `SCU_Drive_Mode_Request` bit 6/2 | 兼容，ROS msg 不暴露 |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-FIELD-001 | P3 | `ScuControlCommand` 没有 header/stamp 字段；当前 driver 不需要该字段，但运行调试只能依赖 topic 时间和 CAN raw 输出。 |
| CHAS-FIELD-002 | P2 | driver 对超范围速度/转角采用“按 0 下发”，control 默认采用 `clamp`。默认参数下 control 27 deg <= driver 27 deg，不触发差异；若未来配置不一致，driver 会把超限值归零。 |

## Impact

- 对规划/控制：control 输出的 SCU 字段足以驱动 chassis_driver 的 0x121 编码。
- 对底盘操作：默认值下不会发布非法 shift，不会用负速度表达倒车，安全停车会变成 `brake=true, speed=0, steering=0`。
- 对实车风险：若 control 与 driver 的最大转角/速度配置被人工改成不一致，driver 会把超限字段置 0，可能造成突变转角或突变速度。

## Recommended Fix

当前不需要立即修复源码。后续可考虑增加一份共享参数或启动前一致性检查，确认 `scu.max_steering_angle_deg <= scu_control_max_steering_angle_deg` 且 `scu.max_target_speed_kmh <= scu_control_max_target_speed_kmh`。

## Verification Method

在 Ubuntu ROS2 环境运行：

```bash
ros2 interface show chassis_interfaces/msg/ScuControlCommand
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/can_tx/raw
```

人工确认：

- `scu_shift_level_request` 只出现 1/2/3。
- 倒车为 `scu_shift_level_request=3` 且 `scu_target_speed >= 0`。
- `abs(scu_steering_angle_front/rear) <= 27.0`。
- stop/estop 时 `scu_brake_enable=true`、速度和转角为 0。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: Windows Codex 环境未运行 `ros2 interface show`、`ros2 topic echo`、`colcon build`。
