# Remaining Fixes Phase 02 Report

- 目标：增加无 ROS2 CLI smoke，覆盖核心规划/控制修复点。
- 变更文件：`scripts/offline_remaining_fixes_smoke.py`。
- 已处理审计发现：A2-R-004、A2-TST-003、A2-TST-004。
- AD Package 兼容性：脚本使用 sample AD Package canonical paths，并生成临时坏包验证 failed validation、bad waypoint index 和 checksum mismatch。
- 安全影响：补充 NaN/Inf guard、estop latch、Ackermann/LQR/MPC 输出配置行为的无 ROS2 回归入口。
- 测试/检查：`offline_remaining_fixes_smoke.py` 已通过；`offline_algorithm_smoke.py` 已通过。
- SKIPPED_ROS2_UNAVAILABLE：`colcon test --packages-select low_speed_av_planning low_speed_av_control`。
- 剩余限制：这是 Python CLI/static smoke，不等价于 C++ 编译或 ROS2 graph runtime 测试。
- 下一步：在 ROS2 环境增加 C++ gtest 或 CLI target。

