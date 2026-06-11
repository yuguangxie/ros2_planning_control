# Chassis Driver CAN 映射审计

## Objective

审计 `chassis_driver` 如何把 `/yunle_chassis/control/scu_control_command` 转换为 CAN `SCU_Control_Command`，确认 CAN ID、bit、scale、drive mode、brake、灯光、valid flags、alive counter 和 checksum 逻辑。

## Scope

- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp`
- `src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp`
- `src/yunle_chassis/Yunle_CAN_release.dbc`
- `src/yunle_chassis/chassis_driver/config/chassis_driver.yaml`
- `src/yunle_chassis/docs/ros2_topic_reference.md`

## Status: Partial

0x121 字段映射与 DBC 静态一致；未发现 alive counter/checksum 信号；driver 只在收到 ROS 消息时发送一次 CAN frame，未发现本地周期发送和控制超时保护。需要台架验证底盘是否要求更严格的周期和 watchdog。

## Evidence

- CAN ID `0x121` 对应 decimal 289：`src/yunle_chassis/Yunle_CAN_release.dbc:124`。
- DBC 信号定义：`src/yunle_chassis/Yunle_CAN_release.dbc:125` 到 `src/yunle_chassis/Yunle_CAN_release.dbc:137`。
- 代码内 DBC 静态表与 DBC 一致：`src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp:78` 到 `src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp:92`。
- driver 生成 `frame.can_id = 289U`、`dlc = 8`：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:82` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:84`。
- driver 固定 `SCU_Drive_Mode_Request` 为 `kDriveAuto=1`：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:16` 和 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:86`。
- 发送通道配置为 `SCU_Control_Command:can2`：`src/yunle_chassis/chassis_driver/config/chassis_driver.yaml:75` 到 `src/yunle_chassis/chassis_driver/config/chassis_driver.yaml:80`。
- 文档说明收到 ROS 消息后立即封装并发送一次：`src/yunle_chassis/docs/ros2_topic_reference.md:77`。
- 文档记录协议周期和当前触发方式差异：`src/yunle_chassis/docs/ros2_topic_reference.md:306` 到 `src/yunle_chassis/docs/ros2_topic_reference.md:307`。
- 文档记录 0x121 周期发送未实现：`src/yunle_chassis/docs/ros2_topic_reference.md:417`。

## ROS 到 CAN 映射表

| ROS 字段 | CAN 信号 | CAN ID | bit/length | factor/offset | endian | driver 处理 |
|---|---|---:|---:|---|---|---|
| `scu_shift_level_request` | `SCU_Shift_Level_Request` | 0x121 | 0/2 | 1/0 | Intel | 只允许 1/2/3，否则丢弃整帧 |
| 无 ROS 输入 | `SCU_Drive_Mode_Request` | 0x121 | 6/2 | 1/0 | Intel | 固定写入 1 |
| `scu_steering_angle_front` | `SCU_Steering_Angle_Front` | 0x121 | 8/8 | 1/0 | Intel | deg 转 `angle/max*120`，负值转 8-bit 补码 |
| `scu_steering_angle_rear` | `SCU_Steering_Angle_Rear` | 0x121 | 16/8 | 1/0 | Intel | 同前轮 |
| `scu_target_speed` | `SCU_Target_Speed` | 0x121 | 24/9 | 0.1/0 | Intel | km/h，负值/超限/非有限按 0 |
| `scu_brake_enable` | `SCU_Brake_Enable` | 0x121 | 33/1 | 1/0 | Intel | true=1 |
| `gw_left_turn_light_request` | `GW_Left_Turn_Light_Request` | 0x121 | 40/2 | 1/0 | Intel | 直接编码 |
| `gw_right_turn_light_request` | `GW_Right_Turn_Light_Request` | 0x121 | 42/2 | 1/0 | Intel | 直接编码 |
| `gw_position_light_request` | `GW_Position_Light_Request` | 0x121 | 46/2 | 1/0 | Intel | 直接编码 |
| `gw_low_beam_request` | `GW_Low_Beam_Request` | 0x121 | 48/2 | 1/0 | Intel | 直接编码 |
| `scu_torque_or_speed_mode` | `SCU_Torque_Or_Speed_Mode` | 0x121 | 58/1 | 1/0 | Intel | 直接编码 |
| `steering_angle_speed_valid` | `Steering_Angle_Speed_Valid` | 0x121 | 60/1 | 1/0 | Intel | true=1 |
| `brake_force_command_valid` | `Brake_Force_Command_Valid` | 0x121 | 61/1 | 1/0 | Intel | true=1 |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-CAN-001 | P2 | driver 内部固定 `SCU_Drive_Mode_Request=1`，这满足项目需求；但 DBC 的 drive mode 值命名需要和底盘供应商确认，避免“Remote/Auto”文本含义误读。 |
| CHAS-CAN-002 | P2 | 未发现 0x121 alive counter 或 checksum 信号；若真实底盘协议另有校验需求，当前 DBC/代码未覆盖。 |
| CHAS-CAN-003 | P2 | driver 当前每收到一条 ROS SCU 消息发送一次 CAN frame，未发现 driver 自己按协议周期持续发送 0x121。 |
| CHAS-CAN-004 | P2 | 未发现 driver 侧控制命令超时停车；如果上游 control 节点退出，driver 不会自行产生 stop 命令。 |

## Impact

- 对控制：control 50 Hz 发布可覆盖 DBC 20 ms 周期需求，但 driver 自身没有独立保底周期。
- 对车辆：如果上游 control 停止发布，driver 不会自动补发 brake stop，这在实车运动场景是安全风险。
- 对联调：需要同时 echo `/yunle_chassis/can_tx/raw` 或使用 CAN 工具确认 0x121 是否按预期频率发送。

## Recommended Fix

本轮不修改源码。后续建议：

1. 和底盘协议确认 0x121 是否要求 alive/checksum。
2. 在 driver 增加控制命令 watchdog，超时发送 brake stop 或停止 drive mode，策略需经底盘确认。
3. 增加 driver 输出频率诊断和最近一次控制命令年龄日志。

## Verification Method

```bash
ros2 topic echo /yunle_chassis/can_tx/raw
ros2 topic hz /yunle_chassis/can_tx/raw
```

如果有 CAN 工具，确认：

- CAN ID 为 `0x121`。
- `SCU_Shift_Level_Request` 按 1/2/3 编码。
- `SCU_Target_Speed` 使用 km/h，scale 0.1。
- `SCU_Brake_Enable` 在 stop/estop 时为 1。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 当前未运行 ROS2/CAN 工具验证。
