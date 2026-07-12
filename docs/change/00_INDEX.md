# 审计后四阶段变更说明索引

本目录说明审计后的 Phase 13、Phase 14、Phase 15、Phase 16 四个阶段分别修改了什么、解决了什么问题，以及这些修改在运行时产生的具体作用。

## 阅读顺序

1. [四阶段总体说明](05_FOUR_PHASE_OVERVIEW.md)：先理解四个阶段之间的依赖关系、端到端数据流和当前完成状态。
2. [Phase 13：Control 端到端安全语义](01_PHASE_13_CONTROL_SAFETY.md)：Planning failure/emergency、VehicleState 门控、锁存急停和双输出合同。
3. [Phase 14：生产代码测试底座与 CI](02_PHASE_14_TEST_INFRASTRUCTURE.md)：production-linked gtest、ROS2 integration source、统一离线入口、模板同步和 GitHub Actions。
4. [Phase 15：Planning 完整性与安全加固](03_PHASE_15_PLANNING_INTEGRITY.md)：AD Package 路径与结构校验、Dijkstra/A*、semantic/trajectory helper 和 Planning ROS2 回归。
5. [Phase 16：Control 工程化与硬件 Watchdog 协同](04_PHASE_16_CONTROL_ENGINEERING.md)：参数治理、真实 dt smoother、controller fail-closed、progress、cadence 和500 ms硬件边界。

## 状态说明

本文档严格区分以下证据状态：

- `PASS`：已在当前环境实际执行并通过。
- `GENERATED_NOT_EXECUTED`：C++测试源码与构建注册已生成，但当前环境没有编译执行。
- `SKIPPED_ROS2_UNAVAILABLE`：因当前 Windows 环境没有 ROS2/colcon而未执行。
- `CONFIGURED_NOT_EXECUTED`：CI或测试流程已配置，但没有当前工作区对应的实际运行结果。
- `EXECUTED_FAIL`：已经执行，但至少一个必需 job失败。
- `HIL_NOT_EXECUTED`：真实底盘或安全台架验证尚未执行。

Python offline smoke主要证明数据合同、fixture、配置和仓库一致性，不等价于生产C++或ROS2行为证明。

## 权威来源

- [Phase 13 Report](../../reports/phase_13_report.md)
- [Phase 14 Report](../../reports/phase_14_report.md)
- [Phase 15 Report](../../reports/phase_15_report.md)
- [Phase 16 Report](../../reports/phase_16_report.md)
- [最终生成报告](../../reports/final_generation_report.md)
- [Finding与风险登记](../audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md)
- [测试、文档与工程化审计](../audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md)

