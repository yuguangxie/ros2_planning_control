# Phase 06 Report
- Goal: 生成控制类型、front/dual Ackermann 车辆模型。
- Files changed: `src/low_speed_av_control/include/low_speed_av_control/*vehicle*`, `src/low_speed_av_control/src/*ackermann*`, `reports/phase_06_report.md`.
- Key design decisions: 车辆模型通过 `VehicleModelFactory` 显式选择。
- AD Package compatibility notes: 支持样例 manifest 中的 `front_ackermann` 和 `dual_ackermann`。
- Config/topic compatibility notes: 默认 `vehicle.model: front_ackermann`，支持列表在 bringup vehicle config 中声明。
- Tests or offline checks run: 后续 smoke 检查控制器输出有限值。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: 未运行控制节点。
- Known limitations: 轮胎几何细节未扩展到每个车轮角。
- Next phase handoff: 生成控制算法。
