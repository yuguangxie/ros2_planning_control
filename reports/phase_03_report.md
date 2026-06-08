# Phase 03 Report
- Goal: 生成 Dijkstra/A* 全局规划。
- Files changed: `src/low_speed_av_planning/include/low_speed_av_planning/*planner*`, `src/low_speed_av_planning/src/dijkstra_planner.cpp`, `src/low_speed_av_planning/src/astar_planner.cpp`, `reports/phase_03_report.md`.
- Key design decisions: 使用显式 factory，不引入 pluginlib；规划结果包含 edge ids、node ids、length、estimated_time。
- AD Package compatibility notes: 拓扑来自 `roadnet/topology.json`，支持 `edge.cost`、`availability`、`blocked_edges`、`allow_reverse`。
- Config/topic compatibility notes: 默认全局算法是 `astar`。
- Tests or offline checks run: 后续 `offline_algorithm_smoke.py` 覆盖 `N0001 -> N0003`。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: 未运行规划 service。
- Known limitations: A* 启发式使用节点欧氏距离。
- Next phase handoff: 生成 motion/speed planners。
