# 第四轮风险登记

## Objective（目标）
登记新增 SCU 输出与 LQR 升级后的剩余风险，按严重度和优先级给出缓解建议。

## Status（状态）
Partial。未发现 P0。存在一个 P1 真实 ROS2/底盘未验证风险，以及多个 P2 集成、枚举和测试风险。

## Evidence（证据）
- ROS2 工具不可用：`scripts/check_ros2_env.ps1` 输出 `SKIPPED_ROS2_UNAVAILABLE`。
- neutral 编码风险：`src/low_speed_av_interfaces/msg/ControlCommand.msg:10` 到 `src/low_speed_av_interfaces/msg/ControlCommand.msg:11`，`src/low_speed_av_control/src/scu_command_mapper.cpp:63` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:72`。
- SCU mapper 通过静态和 Python smoke：`scripts/offline_scu_lqr_smoke.py:173` 到 `scripts/offline_scu_lqr_smoke.py:199`。
- LQR Riccati 源码：`src/low_speed_av_control/src/lqr_controller.cpp:100` 到 `src/low_speed_av_control/src/lqr_controller.cpp:158`。

## Risk Register（风险登记）
| ID | Title | Severity | Probability | Affected module | Evidence | Impact | Recommended mitigation | Priority |
|---|---|---|---|---|---|---|---|---|
| A4-R-001 | ROS2/底盘运行时未验证 | P1 | Medium | control/chassis | ROS2 commands skipped | 可能编译失败、topic 不存在或底盘不接收 | 执行手工 ROS2 验证流程 | High |
| A4-R-002 | `chassis_interfaces` 可用性未知 | P2 | Medium | build/interface | CMake 依赖存在但未构建 | workspace 缺依赖会阻塞构建 | rosdep/workspace 提供包并运行 build | High |
| A4-R-003 | 内部 neutral gear 编码不一致 | P2 | Medium | SCU mapper/interfaces | mapper gear=4，接口注释未列 neutral | N 挡测试或上游命令可能落入 safe stop | 统一 ControlCommand gear 枚举 | High |
| A4-R-004 | 缺少 C++ mapper/LQR 单元测试 | P2 | Medium | testing | Python smoke 通过但非 C++ | C++ 实现与 Python 镜像差异可能漏检 | 添加 gtest/CLI smoke | Medium |
| A4-R-005 | LQR 未经闭环仿真/实车调参 | P2 | Medium | control | 静态与离线通过 | 可能转向过激或收敛慢 | 仿真回放、bench 调参、限制速度/转角 | Medium |
| A4-R-006 | launch 参数加载未验证 | P2 | Medium | bringup/config | ROS2 launch skipped | 节点可能未使用 SCU topic/LQR | `ros2 param get` 验证 | Medium |
| A4-R-007 | 根 README 默认算法描述过时 | P3 | Medium | docs | README 仍写 `pure_pursuit` 默认 | 人工测试可能按旧默认执行 | 更新 README | Low |

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-RG-001 | P1 | Open | 真实 ROS2/底盘验证是最高优先级。 |
| A4-RG-002 | P2 | Open | neutral gear 和 C++ 测试是下一步最明确的工程修复。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
风险主要影响底盘实际接收与车辆安全行为。静态映射通过并不等于 CAN frame 或底盘模式真实正确。

## Recommended fix（推荐修复）
按 `FIX_PLAN_4.md` 阶段执行：先 ROS2 手工验证，再统一 gear 枚举并添加 C++ 测试。

## Verification method（验证方法）
风险关闭需要真实 ROS2 日志、`ros2 interface show`、`ros2 topic echo`、底盘 bench 或 C++ 单元测试结果。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `ros2 interface show`
- `ros2 topic echo`

