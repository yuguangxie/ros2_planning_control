# Remaining Fixes Phase 01 Report

- 目标：实现 RoadnetLoader C++ runtime checksum/hash 校验。
- 变更文件：`src/low_speed_av_planning/src/roadnet_loader.cpp`、`src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp`、`scripts/offline_remaining_fixes_smoke.py`。
- 已处理审计发现：A2-SUM-004、A2-R-002、A2-RL-004、F-RL-004、F-AD-004。
- AD Package 兼容性：继续使用 `project_manifest.json`、`checksums.sha256`、`manifest.hashes` 和 canonical file paths；不使用旧 `manifest.json`、`trajectory/waypoints.json` 或根目录 `validation_report.json`。
- 安全影响：checksum mismatch 现在会抛出明确文件路径错误，不再 warning-only；损坏或篡改包不会进入 active planning。
- 测试/检查：`offline_remaining_fixes_smoke.py` 覆盖 tampered `trajectory/waypoints.yaml` checksum mismatch 拒绝。
- SKIPPED_ROS2_UNAVAILABLE：`colcon build`、`colcon test`、`ros2 service call /low_speed_av_planning/reload_roadnet ...`。
- 剩余限制：C++ 编译未在本机验证；SHA-256 为内置轻量实现，后续 ROS2 环境需 build/test。
- 下一步：补离线 smoke 和语义约束验证。

