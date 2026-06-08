# Remaining Fixes Phase 07 Report

- 目标：补强 motion planner skeleton 安全行为。
- 变更文件：`src/low_speed_av_planning/include/low_speed_av_planning/stop_and_wait_motion_planner.hpp`、`src/low_speed_av_planning/src/stop_and_wait_motion_planner.cpp`、`src/low_speed_av_planning/README.md`。
- 已处理审计发现：A2-R-007、F-PL-004。
- AD Package 兼容性：`stop_and_wait` 仍通过 canonical waypoint index 和 waypoints 拼接 reference-line。
- 安全影响：选择 `stop_and_wait` 不再返回空轨迹或隐含高速轨迹，而是输出显式停车轨迹；`frenet_lite` 和 `hybrid_astar_parking` 仍文档标记为实验 fallback。
- 测试/检查：源码静态检查和 existing offline smoke 通过；后续需 C++ test 覆盖 motion factory。
- SKIPPED_ROS2_UNAVAILABLE：`ros2 service call /low_speed_av_planning/set_planner_algorithm ... motion_planner_algorithm:=stop_and_wait`。
- 剩余限制：`frenet_lite` 与 `hybrid_astar_parking` 仍非完整算法。
- 下一步：为四类 motion planner 添加 C++ 行为测试。

