# Control 到 Yunle Chassis 兼容性总报告

## Objective

基于当前实际代码，审计 `low_speed_av_control` 输出是否能和新增 `src/yunle_chassis/chassis_interfaces`、`src/yunle_chassis/chassis_driver` 正确对应。

## Scope

- `src/low_speed_av_control/`
- `src/low_speed_av_interfaces/`
- `src/low_speed_av_bringup/`
- `src/yunle_chassis/chassis_interfaces/`
- `src/yunle_chassis/chassis_driver/`
- `src/yunle_chassis/Yunle_CAN_release.dbc`
- SCU/Yunle 相关 docs

## Status: Pass For Static Compatibility, Not Verified For Runtime

静态审计显示 control 输出可以被 `chassis_driver` 直接订阅：topic 默认一致、type 完全一致、字段映射和单位一致、安全停车合同一致、27 deg 转角限幅一致。当前 Windows Codex 环境未运行 ROS2，因此 runtime 连接、CAN 实发、底盘响应均为 `Not Verified`。

## Evidence Summary

- control 默认发布 `/yunle_chassis/control/scu_control_command`：`src/low_speed_av_control/src/control_node.cpp:22`。
- control SCU publisher 类型为 `chassis_interfaces::msg::ScuControlCommand`：`src/low_speed_av_control/src/control_node.cpp:99`。
- chassis_driver 订阅 `chassis_interfaces::msg::ScuControlCommand`：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:39`。
- chassis_driver 订阅 topic 由 `/yunle_chassis` + `control/scu_control_command` 得到：`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:141`、`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:435` 到 `src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp:443`。
- SCU msg 只定义 D/N/R 为 1/2/3：`src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:3` 到 `src/yunle_chassis/chassis_interfaces/msg/ScuControlCommand.msg:5`。
- control mapper 将速度转为非负 km/h：`src/low_speed_av_control/src/scu_command_mapper.cpp:73`。
- control mapper 将转角从 rad 转 deg 并限幅：`src/low_speed_av_control/src/scu_command_mapper.cpp:51` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:64`。
- driver 对非法 shift 丢弃整帧：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:44` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:50`。
- driver 将 SCU command 编码到 CAN ID 289/0x121：`src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:82` 到 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp:98`。
- DBC 0x121 字段定义：`src/yunle_chassis/Yunle_CAN_release.dbc:124` 到 `src/yunle_chassis/Yunle_CAN_release.dbc:137`。

## Final Answers

| 问题 | 答案 |
|---|---|
| 1. 当前 control 输出是否能被 `yunle_chassis/chassis_driver` 直接订阅？ | 静态代码层面可以。需要同一 ROS2 workspace 构建并同时启动 control 与 chassis_driver。 |
| 2. topic 名称是否一致？ | 一致，默认均为 `/yunle_chassis/control/scu_control_command`。 |
| 3. msg 类型是否一致？ | 一致，均为 `chassis_interfaces/msg/ScuControlCommand`。 |
| 4. 字段映射是否一致？ | 一致，driver 使用的 0x121 字段均由 control mapper 赋值或安全默认。 |
| 5. 单位是否一致？ | 一致。control 输出 km/h 和 deg，driver 按 km/h 和 deg 编码。 |
| 6. 挡位是否一致？ | 一致。D=1、N=2、R=3；control 不发布 invalid shift。 |
| 7. 安全停车是否一致？ | 一致。control stop 输出合法 shift、speed 0、steering 0、brake true，driver 编码为 0x121 brake/speed/steer。 |
| 8. SCU 27 deg 限幅是否与 driver 一致？ | 一致。control 默认 `scu.max_steering_angle_deg=27.0`，driver 默认 `scu_control_max_steering_angle_deg=27.0`。 |
| 9. 是否还有 P0/P1 阻塞问题？ | 静态兼容无 P0/P1；运行安全上 driver command watchdog 未发现，实车前必须处理或确认。 |
| 10. 是否可以进入 bench-only / wheels-off 验证？ | 可以。 |
| 11. 是否仍禁止真实车辆运动测试？ | 是。未完成 bench 验证和 driver 超时策略确认前禁止。 |
| 12. 下一步优先改哪些文件？ | 若用户确认修复：优先 `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp`、`src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp`、`src/yunle_chassis/chassis_driver/config/chassis_driver.yaml`、`src/low_speed_av_bringup/launch/*.py`。 |

## Top Findings

| ID | Severity | Finding | Impact | Recommended fix |
|---|---|---|---|---|
| CHAS-REP-001 | P2 | driver 只按收到 ROS 消息触发 CAN 发送，未发现本地周期重发 0x121。 | control 抖动或停止时底盘控制链路依赖上游持续发布。 | 增加 driver 周期发送/最近命令保持策略，或确认底盘网关自带 watchdog。 |
| CHAS-REP-002 | P1 | 未发现 driver 侧控制命令超时停车。 | 如果 control 节点退出，driver 不会主动合成 brake stop。 | 增加 command timeout watchdog。 |
| CHAS-REP-003 | P2 | bringup demo 不启动 chassis_driver。 | 用户可能误以为底盘 driver 已连接。 | 增加可选 chassis driver launch 或明确人工启动流程。 |
| CHAS-REP-004 | P2 | control clamp 与 driver overrange-to-zero 策略不同。 | 参数不一致时可能出现 driver 归零。 | 增加参数一致性检查。 |
| CHAS-REP-005 | P3 | `src/yunle_chassis` 当前未跟踪。 | 推送时可能漏掉底盘源码。 | 后续确认后纳入 git。 |

## ROS2 Commands Skipped Or Run

已运行：

```powershell
git status --short
rg -n "ScuControlCommand|scu_control_command|scu_shift_level_request|scu_target_speed|scu_steering_angle_front|scu_drive_mode_request|catkin|roscpp|topic_prefix|scu_control_max_steering_angle_deg|SCU_Control_Command|control/status" src/yunle_chassis src/low_speed_av_control src/low_speed_av_bringup src/low_speed_av_interfaces docs reports
```

未运行：

```text
SKIPPED_ROS2_UNAVAILABLE: colcon build --symlink-install
SKIPPED_ROS2_UNAVAILABLE: ros2 interface show chassis_interfaces/msg/ScuControlCommand
SKIPPED_ROS2_UNAVAILABLE: ros2 topic info /yunle_chassis/control/scu_control_command
SKIPPED_ROS2_UNAVAILABLE: ros2 topic echo /yunle_chassis/control/scu_control_command
SKIPPED_ROS2_UNAVAILABLE: ros2 launch chassis_driver chassis_driver.launch.py
```

## Next Manual Validation

第一条建议在 Ubuntu ROS2 环境执行的命令：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
ros2 launch chassis_driver chassis_driver.launch.py
```

另一个终端启动 planning/control 后执行：

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
```

期望 publisher count = 1、subscriber count = 1、type 为 `chassis_interfaces/msg/ScuControlCommand`。
