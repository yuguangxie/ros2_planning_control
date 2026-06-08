# Remaining Fixes Phase 03 Report

- 目标：让 semantics speed-zone/no-go 影响规划或轨迹速度。
- 变更文件：`src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp`、`src/low_speed_av_planning/src/roadnet_loader.cpp`、`src/low_speed_av_planning/src/planning_node.cpp`、`src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`、`scripts/offline_remaining_fixes_smoke.py`。
- 已处理审计发现：A2-SUM-005、A2-R-003、A2-RL-005、A2-AD-005。
- AD Package 兼容性：解析 `semantics/areas.json` polygon、`type`、`speed_limit_mps`、`allow_planning_through`、`priority`。
- 安全影响：`no_go_area`、`keepout` 或 `allow_planning_through=false` 命中的 edge 会被加入 blocked_edges；speed-zone 命中 waypoint 会降低目标速度。
- 测试/检查：`offline_remaining_fixes_smoke.py` 构造 speed-zone/no-go 临时语义并验证速度降低和路线拒绝。
- SKIPPED_ROS2_UNAVAILABLE：`ros2 service call /low_speed_av_planning/plan_route ...`、`ros2 topic echo /planning/trajectory`。
- 剩余限制：polygon 与 edge 的关系采用 waypoint 命中判定；复杂几何相交仍需后续增强。
- 下一步：增加更多语义边界样例和 C++ 测试。

