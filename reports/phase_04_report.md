# Phase 04 Report
- Goal: 生成 reference_line motion planner 和 speed planners。
- Files changed: `src/low_speed_av_planning/include/low_speed_av_planning/*motion*`, `src/low_speed_av_planning/include/low_speed_av_planning/*speed*`, matching `src/*.cpp`, `reports/phase_04_report.md`.
- Key design decisions: `reference_line` 实现轨迹拼接、边界去重、route_s_m 重算、按位姿裁剪和 horizon 裁剪；其他 motion planner 保持接口骨架。
- AD Package compatibility notes: 轨迹拼接使用 `trajectory/waypoint_index.json` 和 `trajectory/waypoints.yaml`。
- Config/topic compatibility notes: 默认 motion=`reference_line`，speed=`curvature`。
- Tests or offline checks run: 后续离线 smoke 验证轨迹拼接。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: 未运行 `/planning/trajectory` 发布检查。
- Known limitations: `frenet_lite` 和 `hybrid_astar_parking` 是确定性骨架。
- Next phase handoff: 生成 planning node、launch、config。
