# SCU 命令映射审计

## Objective（目标）
审计 `ScuCommandMapper` 对挡位、速度、转角、固定字段、安全停车、非法值和 warning 的处理是否满足 Yunle SCU 协议。

## Status（状态）
Partial。静态实现与离线 smoke 覆盖主要合同；真实 ROS2 topic 输出和底盘驱动接收未验证。内部 neutral gear 编码存在文档不一致风险。

## Evidence（证据）
- SCU shift 常量：`src/low_speed_av_control/src/scu_command_mapper.cpp:10` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:12`。
- rad to deg：`src/low_speed_av_control/src/scu_command_mapper.cpp:13`，`src/low_speed_av_control/src/scu_command_mapper.cpp:42`。
- 速度 m/s 到 km/h 且取绝对值：`src/low_speed_av_control/src/scu_command_mapper.cpp:50` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:57`。
- gear 到 shift：`src/low_speed_av_control/src/scu_command_mapper.cpp:63` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:72`。
- shift 合法性：`src/low_speed_av_control/src/scu_command_mapper.cpp:79` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:86`。
- 安全停车字段：`src/low_speed_av_control/src/scu_command_mapper.cpp:89` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:104`。
- 未知 gear 安全停车：`src/low_speed_av_control/src/scu_command_mapper.cpp:119` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:123`。
- 转角和速度 sanitize：`src/low_speed_av_control/src/scu_command_mapper.cpp:129` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:145`。
- warning 发布：`src/low_speed_av_control/src/control_node.cpp:371` 到 `src/low_speed_av_control/src/control_node.cpp:375`。
- 离线测试覆盖 gear、单位、越界、非有限、estop：`scripts/offline_scu_lqr_smoke.py:173` 到 `scripts/offline_scu_lqr_smoke.py:199`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-SCUM-001 | P3 | Pass | `scu_shift_level_request` 只从 1/2/3 或 sanitized stop shift 产生。 |
| A4-SCUM-002 | P3 | Pass | 未知 gear 会发布安全停车，不会发布非法 shift。 |
| A4-SCUM-003 | P3 | Pass | `speed_mps` 使用 `abs(speed_mps) * 3.6`，倒车由 R 挡表达，不使用负速度。 |
| A4-SCUM-004 | P3 | Pass | 前/后轮转角按 rad 转 degree，并支持 sign 配置。 |
| A4-SCUM-005 | P3 | Pass | 安全停车输出 brake true、速度 0、前后转角 0、灯光 0、valid flags false。 |
| A4-SCUM-006 | P2 | Partial | 内部 `ControlCommand.msg` 注释没有 neutral，mapper 使用 gear=4 表示 neutral；这会影响人工测试和上游模块构造 neutral 命令。 |
| A4-SCUM-007 | P3 | Partial | 越界速度/转角会映射为 0 并 warning；正常路径下 speed=0 不一定 brake true，符合“越界置 0”但需底盘验证是否期望 brake。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
非法 shift 风险静态上已被控制。neutral 编码不一致可能让需要 N 挡的命令退化为默认 D 挡停车。越界速度映射为 0 但 brake false 的正常命令需要确认 Yunle 驱动对 0 speed 的行为。

## Recommended fix（推荐修复）
- 统一内部 gear 枚举，建议在 `ControlCommand.msg` 注释中加入 neutral，或修改 mapper 使用现有 PARK/N 语义。
- 在 bench 上确认 `scu_target_speed=0` 且 `scu_brake_enable=false` 的行为是否安全。
- 增加 C++ 单元测试直接覆盖 `ScuCommandMapper`，不要只依赖 Python 镜像。

## Verification method（验证方法）
- 已执行 `offline_scu_lqr_smoke.py`，通过。
- 静态核查 mapper 源码。
- 未在 ROS2 topic 上观测实际 `ScuControlCommand`。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 topic echo /yunle_chassis/control/scu_control_command`
- `ros2 topic hz /yunle_chassis/control/scu_control_command`

