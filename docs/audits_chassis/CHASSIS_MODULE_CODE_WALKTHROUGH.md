# Yunle Chassis 模块代码走读

## Objective

说明 `src/yunle_chassis` 中 `chassis_interfaces` 与 `chassis_driver` 的模块结构、接口定义、topic 订阅、CAN 编码和与 `low_speed_av_control` 的关系。

## Scope

- `src/yunle_chassis/chassis_interfaces`
- `src/yunle_chassis/chassis_driver`
- `src/yunle_chassis/Yunle_CAN_release.dbc`
- 与 SCU command 有关的 control 代码

## Status: Pass With Not Verified Runtime

代码结构清晰，ROS2/ament 构建文件存在，SCU command 桥接路径完整。当前 Windows 环境未运行 ROS2 构建和 launch，运行行为仍需 Ubuntu 验证。

## Evidence

- `chassis_interfaces` 生成 `ScuControlCommand.msg`：`src/yunle_chassis/chassis_interfaces/CMakeLists.txt:13` 到 `src/yunle_chassis/chassis_interfaces/CMakeLists.txt:30`。
- `chassis_driver_node` 目标包含 `control_command_bridge.cpp`、`dbc_protocol.cpp`、UDP/CAN 编解码文件：`src/yunle_chassis/chassis_driver/CMakeLists.txt:19` 到 `src/yunle_chassis/chassis_driver/CMakeLists.txt:27`。
- driver include 引入 `scu_control_command.hpp`：`src/yunle_chassis/chassis_driver/include/chassis_driver/chassis_driver_node.hpp:12`。
- driver 构造时加载参数、创建 pub/sub、初始化通道和桥接器：`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:84` 到 `src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:127`。
- SCU command bridge 订阅 `control/scu_control_command` 并编码 CAN：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:38` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:98`。
- DBC 表内包含 0x121 `SCU_Control_Command`：`src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp:78` 到 `src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp:92`。

## 模块结构

| 模块 | 作用 | 关键文件 |
|---|---|---|
| `chassis_interfaces` | ROS2 msg 定义 | `src/yunle_chassis/chassis_interfaces/msg/*.msg` |
| `chassis_driver` | ROS2 topic 与 UDP-CAN 网关桥接 | `src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp` |
| `ControlCommandBridge` | 控制 topic 到 CAN frame 编码 | `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp` |
| `DbcProtocol` | DBC 信号抽取/插入工具与静态 DBC 表 | `src/yunle_chassis/chassis_driver/src/dbc_protocol.cpp` |
| `keyboard_scu_control_node` | 人工发布 SCU 命令用于低速联调 | `src/yunle_chassis/chassis_driver/src/keyboard_scu_control_node.cpp` |

## 关键数据流

```text
low_speed_av_control
  -> /yunle_chassis/control/scu_control_command
  -> chassis_driver ControlCommandBridge
  -> DbcProtocol::encodeSignal
  -> CAN 0x121 SCU_Control_Command
  -> UDP CAN gateway
```

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-WALK-001 | P3 | `keyboard_scu_control_node` 也会发布 `/yunle_chassis/control/scu_control_command`，bench 时需避免它和 `low_speed_av_control` 同时发布造成双 publisher。 |
| CHAS-WALK-002 | P2 | driver 发送 `SCU_Drive_Mode_Request=1` 是内部固定逻辑，不在 ROS msg 暴露；此点与项目需求一致，但必须在人工验证中确认底盘协议对值 1 的解释。 |
| CHAS-WALK-003 | P2 | driver 文档显示 0x121 当前由 ROS callback 触发一次发送，未发现 driver 内部周期发送。 |

## Impact

- 对控制：control 是 SCU topic 的推荐自动驾驶 publisher。
- 对手动调试：keyboard 节点适合单独 bench 使用，不能和 control 同时争用同一 topic。
- 对车辆：CAN 0x121 编码链路明确，但周期/timeout 需要实测。

## Recommended Fix

本轮不修改。建议后续文档明确“同一时间只允许一个 SCU command publisher”，并在 launch 层避免 keyboard 与 control 同时启动。

## Verification Method

```bash
ros2 node list
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/can_tx/raw
```

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 未运行 ROS2 命令。
