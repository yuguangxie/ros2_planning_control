# Risk Register 5

## Objective

记录第 5 轮完整模块审计后的主要风险、概率、影响和缓解措施。

## Scope

全工程。

## Status

Partial。

## Evidence

- `docs/audits_5/AUDIT_5_SUMMARY.md` 汇总了第五轮审计的模块状态、离线检查结果和未执行的 ROS2 验证项。
- `docs/audits_5/PLANNING_MODULE_FUNCTION_AUDIT.md`、`docs/audits_5/CURRENT_POSE_START_PLANNING_AUDIT.md`、`docs/audits_5/CONTROL_MODULE_FUNCTION_AUDIT.md`、`docs/audits_5/SIMULATION_VISUALIZATION_AUDIT.md` 分别给出了规划、当前位置起点、控制和仿真模块的代码证据。
- `docs/audits_5/ROS2_MANUAL_VALIDATION_PROCEDURE.md` 列出了仍需在真实 ROS2/Ubuntu 环境中执行的人工验证命令。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-RISK-001 | P1 | Open | 最高优先级风险集中在 ROS2 未验证、仿真包未实际编译、SCU 底盘未 bench 验证、仿真定位误接真实车辆。 | 可能影响构建可用性、规划控制闭环和车辆安全。 | 按本风险登记表优先级执行 ROS2 build、launch、bench 和安全隔离验证。 | 更新本表风险状态并附 ROS2/bench 日志。 |

## Risks

| ID | Title | Severity | Probability | Affected module | Evidence | Impact | Recommended mitigation | Priority |
|---|---|---|---|---|---|---|---|---|
| R5-001 | ROS2 runtime not verified | P1 | High | all | 当前环境无 `colcon`/`ros2` | 可能存在构建、launch、topic/service 问题 | 在 Ubuntu/ROS2 执行完整人工验证 | 1 |
| R5-002 | Simulation package compile/link not verified | P1 | Medium | simulation | 新增包仅静态/离线验证 | 可视化模块可能无法 build 或 launch | `colcon build --packages-select low_speed_av_simulation` | 1 |
| R5-003 | Chassis bench not verified | P1 | Medium | control/SCU | 未运行真实 chassis driver | SCU 字段或周期不匹配可能导致底盘不响应 | bench/wheels-off 验证 | 1 |
| R5-004 | Current-pose matcher edge cases | P2 | Medium | planning | waypoint 近邻 + progress 启发式 | 交叉口或边中段可能起点不理想 | 实现 edge projection 和 route crop | 2 |
| R5-005 | Simulated localization used with live vehicle | P1 | Medium | simulation/control | sim node 发布真实 `/localization/pose` | 可能触发真实底盘命令 | namespace/safety gate/文档警告/bench only | 1 |
| R5-006 | RViz frame mismatch | P2 | Medium | simulation | frame 默认 `map`，依赖外部 TF/RViz fixed frame | markers 不显示或错位 | 手工确认 frame，必要时发布 static transform | 2 |
| R5-007 | Route starts behind vehicle or wrong direction | P2 | Medium | planning | heading 阈值与 reverse edge 简化处理 | 起步路线不符合车辆运动方向 | 加强方向/gear-aware matching | 2 |
| R5-008 | Trajectory timestamp/frame mismatch | P2 | Medium | planning/control/simulation | 轨迹 msg 无显式 frame 字段，依赖 header | 控制或 RViz 解释不一致 | 增加文档和 ROS2 integration check | 3 |
| R5-009 | LQR tuning not validated on real vehicle | P2 | High | control | 仅离线有限性验证 | 跟踪质量未知 | 低速调参和闭环仿真 | 3 |
| R5-010 | SCU field mismatch with actual driver | P1 | Medium | SCU output | 未 `ros2 interface show` | 编译或运行失败 | 确认 `chassis_interfaces` 版本 | 1 |

## ROS2 Commands Run Or Skipped

SKIPPED_ROS2_UNAVAILABLE:

- all ROS2 runtime validation commands.

## Remaining Uncertainty

风险概率需在真实 ROS2 和 bench 测试后更新。
