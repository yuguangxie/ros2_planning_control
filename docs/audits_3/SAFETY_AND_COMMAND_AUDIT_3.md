# 安全与命令审计 3

## Objective（目标）
审计 control 安全停车、localization timeout、trajectory timeout、empty trajectory stop、estop 优先级、estop latch/clear 策略、命令限幅、平滑和 NaN/Inf guard。

## Status（状态）
Partial。源码层面的安全策略清晰，estop 优先级最高，命令会经限幅和平滑；但实际 ROS2 时间、topic 延迟、消息等级和恢复行为未运行验证。

## Evidence（证据）
- safety 配置加载：`src/low_speed_av_control/src/control_node.cpp:99` 到 `src/low_speed_av_control/src/control_node.cpp:105`。
- controlled stop 构造：`src/low_speed_av_control/src/control_node.cpp:143`。
- safety status callback 和 latch/clear：`src/low_speed_av_control/src/control_node.cpp:188` 到 `src/low_speed_av_control/src/control_node.cpp:199`。
- timer 中 estop 优先：`src/low_speed_av_control/src/control_node.cpp:235` 到 `src/low_speed_av_control/src/control_node.cpp:236`。
- localization timeout：`src/low_speed_av_control/src/control_node.cpp:240` 到 `src/low_speed_av_control/src/control_node.cpp:243`。
- trajectory timeout：`src/low_speed_av_control/src/control_node.cpp:247` 到 `src/low_speed_av_control/src/control_node.cpp:250`。
- empty trajectory stop：`src/low_speed_av_control/src/control_node.cpp:253` 到 `src/low_speed_av_control/src/control_node.cpp:255`。
- normal command：`src/low_speed_av_control/src/control_node.cpp:259`。
- finite guard：`src/low_speed_av_control/src/command_limiter.cpp:11` 到 `src/low_speed_av_control/src/command_limiter.cpp:16`，`src/low_speed_av_control/src/command_limiter.cpp:34`。
- estop config 默认 latch：`src/low_speed_av_control/config/control_params.yaml:39`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-SC-001 | P3 | Pass | estop 优先于 timeout 和正常跟踪命令。 |
| A3-SC-002 | P3 | Pass | localization/trajectory timeout 和 empty trajectory 均输出 controlled stop。 |
| A3-SC-003 | P3 | Pass | NaN/Inf guard 会将非有限命令转为安全停车并标记 `nan_or_inf_guard`。 |
| A3-SC-004 | P2 | Partial | estop latch/clear 策略已实现，但缺少 ROS2 topic 时序测试和恢复验收记录。 |
| A3-SC-005 | P2 | Not Verified | 命令平滑器在真实控制频率、时间戳和连续 topic 输入下未验证。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
安全链路设计合理，能降低 stale command、空轨迹和异常数值带来的风险。未验证的运行时行为仍可能影响实车恢复、清除 estop 和连续命令平滑。

## Recommended fix（推荐修复）
- 增加 ROS2 integration test：发布 safety level >= 2，确认 `/control/command.emergency_stop=true`，再发布 clear 状态确认恢复策略。
- 添加 C++ 或 Python 更细粒度的 latch/clear 状态机测试。
- 在实车前用仿真验证 limiter/smoother 对连续命令的响应。

## Verification method（验证方法）
- `offline_algorithm_smoke.py` 覆盖 estop OK。
- `offline_remaining_fixes_smoke.py` 覆盖 estop latch clear policy。
- 未运行 ROS2 topic 时序测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus ...`
- `ros2 topic echo /control/command`
- `ros2 topic echo /control/status`

