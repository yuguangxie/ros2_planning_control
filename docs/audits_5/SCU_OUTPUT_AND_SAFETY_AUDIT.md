# SCU Output And Safety Audit

## Objective

审计 Yunle SCU 输出映射、安全停车、shift 合法性、单位转换和异常值保护。

## Scope

- `src/low_speed_av_control/src/scu_command_mapper.cpp`
- `src/low_speed_av_control/include/low_speed_av_control/scu_command_mapper.hpp`
- `src/low_speed_av_control/src/control_node.cpp`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`

## Status

Pass by static/offline audit, Not Verified with real chassis driver.

## Evidence

- 默认 SCU topic：`src/low_speed_av_control/src/control_node.cpp:22`。
- SCU publisher 类型：`src/low_speed_av_control/src/control_node.cpp:97`。
- 默认 valid flags：`src/low_speed_av_control/src/scu_command_mapper.cpp:22` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:24`。
- Steering sanitize：`src/low_speed_av_control/src/scu_command_mapper.cpp:43`。
- Speed sanitize：`src/low_speed_av_control/src/scu_command_mapper.cpp:56`。
- Stop command：`src/low_speed_av_control/src/scu_command_mapper.cpp:89` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:100`。
- Normal SCU fields：`src/low_speed_av_control/src/scu_command_mapper.cpp:128` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:141`。
- 文档映射：`docs/YUNLE_SCU_COMMAND_OUTPUT.md:55` 到 `docs/YUNLE_SCU_COMMAND_OUTPUT.md:82`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-SCU-001 | P3 | Pass | 默认 topic 精确为 `/yunle_chassis/control/scu_control_command`。 | 满足底盘 driver 合同。 | 无。 | `ros2 topic info`。 |
| AUD5-SCU-002 | P3 | Pass | stop command 使用有效 stop shift，speed=0，steering=0，brake=true。 | 安全停车输出合理。 | 无。 | estop/timeout echo。 |
| AUD5-SCU-003 | P3 | Pass | speed 转 km/h 并 sanitize；steering 转 deg 并 sanitize。 | 避免非法速度/转角。 | 无。 | SCU mapper unit tests/echo。 |
| AUD5-SCU-004 | P1 | Not Verified | 未在真实 `chassis_interfaces` 与 chassis driver 中验证字段和 CAN 输出。 | 真实底盘可能不接受命令或语义不同。 | bench 模式验证 driver 接收。 | `ros2 interface show` + driver logs/CAN。 |
| AUD5-SCU-005 | P0 | Pass by static search | 源码未引用 `scu_drive_mode_request`。 | 避免发布不存在字段。 | 无。 | `rg scu_drive_mode_request src`。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py`
- `rg -n "scu_drive_mode_request" .`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 interface show chassis_interfaces/msg/ScuControlCommand`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`
- chassis bench/CAN validation

## Remaining Uncertainty

真实底盘 driver 的字段解释、周期要求、CAN 输出和安全互锁未验证。

