# 第四轮后续修复计划

## Objective（目标）
为 SCU 输出和 LQR 控制器的剩余风险给出分阶段修复与验证计划。

## Status（状态）
Pass。计划覆盖 P1/P2 风险，优先级以真实 ROS2/底盘验证为先。

## Evidence（证据）
- 风险登记：`docs/audits_4/RISK_REGISTER_4.md`。
- 手工流程：`docs/audits_4/ROS2_MANUAL_VALIDATION_PROCEDURE.md`。
- SCU mapper 与 LQR 源码静态通过。

## Fix Plan（修复计划）
| Phase | Target | Files likely affected | Exact fix description | Acceptance criteria | Suggested Codex prompt for that fix phase |
|---|---|---|---|---|---|
| 1 | 真实 ROS2 验证 | `docs/audits_4/ROS2_MANUAL_VALIDATION_PROCEDURE.md`, logs | 在 ROS2 环境执行构建、接口、launch、topic、safety、LQR 测试。 | 有完整 pass/fail 表，SCU topic 可 echo，build/test 成功或失败原因明确。 | “在真实 ROS2 环境执行第四轮手工验证流程，记录输出并修复编译/接口/launch 问题。” |
| 2 | 统一 gear 枚举 | `ControlCommand.msg`, `TrajectoryPoint.msg`, `ScuCommandMapper`, docs/tests | 明确 neutral 内部编码，更新 mapper 和文档，补测试。 | drive/neutral/reverse 映射 1/2/3 明确且无歧义。 | “统一内部 gear 枚举与 SCU shift 映射，修复 neutral 语义风险。” |
| 3 | C++ 单元测试 | `test/*.cpp`, `CMakeLists.txt` | 添加 `ScuCommandMapper`、`LqrController`、Ackermann、limiter/smoother 的 gtest 或 CLI smoke。 | C++ 测试覆盖非法 gear、越界/非有限、LQR Q/R、低速、双 Ackermann。 | “为 SCU mapper 和 LQR 添加 C++ gtest/CLI smoke，不依赖 ROS graph。” |
| 4 | LQR 仿真调参 | config/docs/test data | 使用离线轨迹或仿真数据调整 Q/R、preview、速度和转角限制。 | LQR 横向误差收敛、输出平滑、无明显振荡。 | “基于仿真或数据回放调优 LQR 参数，生成调参报告。” |
| 5 | 底盘 bench 验证 | validation logs/docs | wheels-off 或 bench 验证 Yunle driver 接收 topic、CAN 0x121、D/R/Brake 行为。 | 底盘确认接收 SCU 命令，安全停车有效。 | “在 bench 环境验证 Yunle SCU topic 到底盘驱动/CAN 行为，记录安全结果。” |

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-FP-001 | P1 | Planned | 第一步必须是真实 ROS2 验证，不能由离线脚本替代。 |
| A4-FP-002 | P2 | Planned | gear 枚举统一和 C++ 测试能显著降低集成误解。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
执行该计划后，SCU 输出可从静态可行提升为 ROS2/底盘可验证，LQR 可从算法源码通过提升为仿真/bench 可用。

## Recommended fix（推荐修复）
按 Phase 1 到 Phase 3 优先执行。Phase 4/5 需要真实车辆或仿真资源。

## Verification method（验证方法）
每阶段都需保存命令输出、日志和 pass/fail 表。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- 当前环境未执行 Phase 1/5 的 ROS2 和底盘命令。

