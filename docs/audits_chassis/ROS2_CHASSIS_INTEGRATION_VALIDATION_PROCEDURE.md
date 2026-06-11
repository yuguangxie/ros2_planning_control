# ROS2 Yunle Chassis 集成人工验证流程

## Objective

提供人类操作员在 Ubuntu ROS2 环境下验证 `low_speed_av_control` 与 `yunle_chassis/chassis_driver` 对接的完整流程。

## Scope

- ROS2 Humble workspace
- `chassis_interfaces`
- `chassis_driver`
- `low_speed_av_control`
- `low_speed_av_bringup`
- bench-only / wheels-off 安全验证

## Status: Not Verified

此文档是人工测试流程。Windows Codex 环境未执行 ROS2 命令，所有 ROS2 命令在本轮审计中均为 `SKIPPED_ROS2_UNAVAILABLE`。

## Evidence

- control 默认 SCU topic：`src/low_speed_av_control/src/control_node.cpp:22`。
- driver 默认订阅 SCU topic：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:39` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:40`。
- chassis driver launch：`src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py:13` 到 `src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py:20`。
- planning/control demo launch 不启动 chassis driver：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:32` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:48`。

## A. 安全前置条件

| 项目 | 要求 |
|---|---|
| 车辆状态 | bench-only、车轮离地或整车禁能 |
| 急停 | 物理急停可用，操作员在场 |
| 网络 | CAN 网关 IP/端口确认，不接实车时可只验证 ROS topic |
| publisher | 同一时刻只允许一个 `/yunle_chassis/control/scu_control_command` publisher |
| 速度 | 首轮验证只允许低速和 brake stop |
| 真实车辆 | 本流程通过前禁止实车运动测试 |

## B. 构建验证

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

人工确认：

- `chassis_interfaces` 接口生成成功。
- `chassis_driver` 构建成功。
- `low_speed_av_control` 构建成功。
- 无 `catkin` 或 `roscpp` 依赖错误。

## C. 接口验证

```bash
ros2 interface show chassis_interfaces/msg/ScuControlCommand
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
```

人工确认：

- `ScuControlCommand` 包含 shift、前/后转角、target speed、brake、lights、mode、valid flags。
- `ScuControlCommand` 不包含 `scu_drive_mode_request`。
- `ControlCommand` 内部命令保留 front/rear steering 字段。

## D. 启动 planning/control

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1
```

确认：

```bash
ros2 node list
ros2 param get /low_speed_av_control topics.scu_command_topic
ros2 param get /low_speed_av_control output.mode
ros2 param get /low_speed_av_control scu.max_steering_angle_deg
ros2 param get /low_speed_av_control scu.max_target_speed_kmh
```

期望：

- `/low_speed_av_control` 存在。
- `topics.scu_command_topic` 为 `/yunle_chassis/control/scu_control_command`。
- `output.mode` 为 `scu_control_command` 或 `both`。
- `scu.max_steering_angle_deg=27.0`。

## E. 启动 chassis driver

```bash
ros2 launch chassis_driver chassis_driver.launch.py
```

确认：

```bash
ros2 node list
ros2 param get /chassis_driver_node topic_prefix
ros2 param get /chassis_driver_node default_qos_depth
ros2 param get /chassis_driver_node scu_control_max_steering_angle_deg
ros2 param get /chassis_driver_node scu_control_max_target_speed_kmh
```

期望：

- `/chassis_driver_node` 存在。
- `topic_prefix=/yunle_chassis`。
- `default_qos_depth=10`。
- `scu_control_max_steering_angle_deg=27.0`。

## F. Topic 连接验证

```bash
ros2 topic list
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- topic type 是 `chassis_interfaces/msg/ScuControlCommand`。
- publisher count = 1。
- subscriber count = 1。
- 如果 keyboard 节点也在运行，publisher count 可能大于 1，应停止 keyboard 节点。

## G. D 挡前进样本

通过规划/控制产生 D 挡命令，或在 bench-only 下临时发布一次：

```bash
ros2 topic pub --once /yunle_chassis/control/scu_control_command \
  chassis_interfaces/msg/ScuControlCommand \
  "{scu_shift_level_request: 1, scu_steering_angle_front: 0.0, scu_steering_angle_rear: 0.0, scu_target_speed: 1.0, scu_brake_enable: false, gw_left_turn_light_request: 0, gw_right_turn_light_request: 0, gw_position_light_request: 0, gw_low_beam_request: 0, scu_torque_or_speed_mode: 1, steering_angle_speed_valid: false, brake_force_command_valid: false}"
```

人工确认：

- shift = 1。
- speed 非负 km/h。
- steering deg。
- driver 无 invalid shift 日志。

## H. R 挡倒车样本

```bash
ros2 topic pub --once /yunle_chassis/control/scu_control_command \
  chassis_interfaces/msg/ScuControlCommand \
  "{scu_shift_level_request: 3, scu_steering_angle_front: 0.0, scu_steering_angle_rear: 0.0, scu_target_speed: 1.0, scu_brake_enable: false, gw_left_turn_light_request: 0, gw_right_turn_light_request: 0, gw_position_light_request: 0, gw_low_beam_request: 0, scu_torque_or_speed_mode: 1, steering_angle_speed_valid: false, brake_force_command_valid: false}"
```

人工确认：

- reverse 通过 shift=3 表达。
- `scu_target_speed` 仍为非负。

## I. Brake Stop 样本

```bash
ros2 topic pub --once /yunle_chassis/control/scu_control_command \
  chassis_interfaces/msg/ScuControlCommand \
  "{scu_shift_level_request: 1, scu_steering_angle_front: 0.0, scu_steering_angle_rear: 0.0, scu_target_speed: 0.0, scu_brake_enable: true, gw_left_turn_light_request: 0, gw_right_turn_light_request: 0, gw_position_light_request: 0, gw_low_beam_request: 0, scu_torque_or_speed_mode: 1, steering_angle_speed_valid: false, brake_force_command_valid: false}"
```

人工确认：

- brake true。
- speed 0。
- front/rear steering 0。
- shift 合法。

## J. Estop/Timeout 验证

触发 safety estop：

```bash
ros2 topic pub --once /safety/status low_speed_av_interfaces/msg/ModuleStatus \
  "{module_name: 'manual_test', state: 'estop', level: 2, message: 'manual estop'}"
```

观察：

```bash
ros2 topic echo /control/status
ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- `/control/status` 进入 `safety_estop` 或 stopping。
- SCU 命令 `scu_brake_enable=true`、速度 0、转角 0。

## K. CAN 输出验证

```bash
ros2 topic echo /yunle_chassis/can_tx/raw
ros2 topic hz /yunle_chassis/can_tx/raw
```

如果有外部 CAN 工具，确认：

- CAN ID `0x121`。
- `SCU_Shift_Level_Request` 为 1/2/3。
- `SCU_Target_Speed` scale 0.1 km/h。
- `SCU_Drive_Mode_Request` 由 driver 内部固定。

## L. Pass/Fail 表

| 步骤 | 命令 | 期望输出 | 人工确认 | Pass/Fail | Notes |
|---|---|---|---|---|---|
| Build | `colcon build --symlink-install` | 所有包构建成功 | 无缺依赖 |  |  |
| Interface | `ros2 interface show chassis_interfaces/msg/ScuControlCommand` | 字段匹配 | 无 drive mode 字段 |  |  |
| Topic info | `ros2 topic info /yunle_chassis/control/scu_control_command` | pub=1, sub=1 | type 正确 |  |  |
| D sample | topic pub/echo | shift=1 speed>=0 | driver 无警告 |  |  |
| R sample | topic pub/echo | shift=3 speed>=0 | 无负速度 |  |  |
| Stop | topic pub/echo | brake true speed 0 steering 0 | CAN 0x121 对应 |  |  |
| Estop | `/safety/status` | stop command | status 有原因 |  |  |
| Steering limit | echo SCU | abs steering <=27 | 无 overrange |  |  |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-VAL-001 | P1 | 未完成真实 ROS2 topic 连接验证前，不得认为 control-driver 已经运行成功。 |
| CHAS-VAL-002 | P0 | 未完成 bench-only / wheels-off 验证前，禁止真实车辆运动测试。 |

## Impact

本流程通过后可以进入更高层的台架闭环验证；未通过前只能做静态审计和仿真。

## Recommended Fix

若验证失败，优先检查：

1. `topics.scu_command_topic`
2. `topic_prefix`
3. `chassis_interfaces` 是否同一 workspace 版本
4. publisher/subscriber count
5. driver 日志中的 invalid shift 或 overrange warning

## Verification Method

按本文件 A 到 L 执行。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 本轮未执行以上 ROS2 命令。
