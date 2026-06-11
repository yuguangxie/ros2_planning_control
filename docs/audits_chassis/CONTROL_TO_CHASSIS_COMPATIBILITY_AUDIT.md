# Control 到 Yunle Chassis 兼容性审计

## Objective

判断 `low_speed_av_control` 当前最终输出能否被 `src/yunle_chassis/chassis_driver` 直接订阅，并确认 topic、消息类型、QoS、字段、单位、挡位、安全停车和限幅是否匹配。

## Scope

- `src/low_speed_av_control/`
- `src/yunle_chassis/chassis_interfaces/`
- `src/yunle_chassis/chassis_driver/`
- `src/low_speed_av_bringup/`
- SCU/Yunle 相关 docs 和报告

## Status: Pass With Runtime Verification Required

静态审计显示 control 和 chassis_driver 默认 topic/type/QoS/字段/单位兼容，可以进入 bench-only 或 wheels-off 验证；不得直接进入真实车辆运动测试。

## Evidence

- Control 默认 SCU topic：`src/low_speed_av_control/src/control_node.cpp:22`。
- Control 默认 output mode：`src/low_speed_av_control/src/control_node.cpp:24`。
- Control SCU publisher 类型和 topic：`src/low_speed_av_control/src/control_node.cpp:99` 到 `src/low_speed_av_control/src/control_node.cpp:100`。
- Control SCU 配置默认 27 deg、5 km/h、`clamp`：`src/low_speed_av_control/src/control_node.cpp:69` 到 `src/low_speed_av_control/src/control_node.cpp:73`。
- Bringup control YAML 使用同一 SCU topic：`src/low_speed_av_bringup/config/control_params.yaml:15`。
- Chassis driver 使用 topic prefix `/yunle_chassis`：`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:141`。
- Chassis driver 使用 `makeTopicName("control/scu_control_command")` 订阅：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:39` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:40`。
- `makeTopicName` 拼接 prefix 和 suffix：`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:435` 到 `src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:443`。
- Chassis driver config 默认 `topic_prefix: /yunle_chassis`：`src/yunle_chassis/chassis_driver/config/chassis_driver.yaml:11`。
- Chassis driver QoS depth 默认 10：`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:146`。
- Control publisher QoS depth 10：`src/low_speed_av_control/src/control_node.cpp:99` 到 `src/low_speed_av_control/src/control_node.cpp:100`。
- Chassis subscriber QoS depth 10：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:36`。

## 重点问题回答

| 问题 | 结论 | 证据 |
|---|---|---|
| control 最终发布 topic | 默认 `/yunle_chassis/control/scu_control_command` | `src/low_speed_av_control/src/control_node.cpp:22` |
| 发布消息类型 | `chassis_interfaces/msg/ScuControlCommand` | `src/low_speed_av_control/src/control_node.cpp:99` |
| driver 实际订阅 topic | 默认 `/yunle_chassis/control/scu_control_command` | `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:39` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:40` |
| driver 订阅类型 | `chassis_interfaces::msg::ScuControlCommand` | `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:39` |
| topic 名称是否一致 | 是 | control 参数和 driver prefix/suffix 拼接一致 |
| QoS 是否兼容 | 静态兼容 | 两侧都是 KeepLast(10)，未配置特殊 durability/reliability |
| control 字段是否覆盖 driver 需要字段 | 是 | `src/low_speed_av_control/src/scu_command_mapper.cpp:153` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:176` |
| 未设置字段是否安全 | 消息字段均有赋值或默认构造；stop 强制灯光和 valid flags 为安全默认 | `src/low_speed_av_control/src/scu_command_mapper.cpp:115` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:131` |
| `scu_drive_mode_request` 是否不存在于 ROS msg | 是；driver 内部固定编码 | msg 无字段，driver 在 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:86` 固定编码 |
| 是否可以 bench-only 验证 | 可以 | 无 P0/P1 静态阻塞，但需 ROS2/底盘台架验证 |

## Topic 兼容表

| 模块 | topic | type | QoS | 参数名 | 默认值 | 是否匹配 |
|---|---|---|---|---|---|---|
| low_speed_av_control publisher | `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | KeepLast(10) | `topics.scu_command_topic` | `/yunle_chassis/control/scu_control_command` | 是 |
| chassis_driver subscriber | `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | KeepLast(10) | `topic_prefix` + relative suffix | `/yunle_chassis` + `control/scu_control_command` | 是 |

## 单位和范围兼容表

| 项目 | control 输出 | driver 期望 | 是否一致 | 风险 |
|---|---|---|---|---|
| 速度 | `abs(speed_mps) * 3.6` km/h | km/h，非负 | 是 | control 默认最大 5 km/h，driver 接受 15 km/h，安全偏保守 |
| 倒车方向 | shift=3，速度非负 | shift=3，速度非负 | 是 | 无 |
| 前/后转角 | rad -> deg，默认 clamp 到 27 deg | deg，范围 ±27 deg | 是 | 若配置不一致，driver 会归零超限转角 |
| 非有限值 | mapper 置 0 或 stop；limiter 也有 NaN/Inf guard | driver 置 0 | 是 | 双重保护 |
| invalid shift | mapper 变安全停车，不发布 invalid shift | driver 丢弃 invalid shift 整帧 | 是 | control 与 driver 双重保护 |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-COMPAT-001 | P2 | `low_speed_av_bringup` 的 demo launch 只启动 planning/control，没有同时启动 `chassis_driver_node`，需要人工另起 driver。 |
| CHAS-COMPAT-002 | P2 | driver 当前按每条 ROS SCU 消息触发一次 CAN 发送，没有在 driver 内部发现 0x121 周期重发或控制超时停车 watchdog。 |
| CHAS-COMPAT-003 | P2 | control 侧速度最大值默认 5 km/h，driver 最大接受 15 km/h；这是保守设置，但需要在联调表中明确不要将 control 配得大于 driver。 |
| CHAS-COMPAT-004 | P3 | `src/yunle_chassis` 当前在 git 状态中显示为未跟踪目录，交付前需要确认是否纳入版本控制。 |

## Impact

- 对控制/底盘通信：默认配置下可以直接通信。
- 对实车操作：仍不允许直接带轮运动测试，因为 driver 侧未发现本地命令超时停车，且真实 CAN 网关/底盘未在当前环境验证。
- 对运维：需要明确启动顺序：planning/control、simulation 或定位、再单独启动 chassis_driver。

## Recommended Fix

本阶段只审计，不修改源码。后续优先修复：

1. 新增 bringup 或文档化的 `planning_control_chassis_demo.launch.py`，显式可选拉起 chassis_driver。
2. 在 chassis_driver 增加命令超时 watchdog，超时发送 brake stop 或停止控制帧，具体策略需和底盘协议确认。
3. 增加启动前参数一致性检查或诊断输出。

## Verification Method

Ubuntu ROS2 台架验证：

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/can_tx/raw
ros2 param get /low_speed_av_control topics.scu_command_topic
ros2 param get /chassis_driver_node topic_prefix
```

期望：

- publisher count = 1。
- subscriber count = 1。
- type 为 `chassis_interfaces/msg/ScuControlCommand`。
- D/R/stop 样本字段满足安全合同。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 当前 Windows Codex 环境未运行 ROS2 CLI。
