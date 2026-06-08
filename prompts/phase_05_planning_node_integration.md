# Phase 05 — Integrate Planning ROS2 Node

请实现 `low_speed_av_planning_node`。

要求：

1. 参数化加载 AD Package：`roadnet.package_path`。
2. 默认定位 topic `/localization/pose`，可配置：`topics.localization_pose_topic`。
3. 支持 `geometry_msgs/msg/PoseStamped` 输入。
4. 发布 `/planning/global_route`。
5. 发布 `/planning/trajectory`。
6. 发布 `/planning/status`。
7. 发布 `/planning/roadnet_status`。
8. 提供 `ReloadRoadnet.srv`。
9. 提供 `PlanRoute.srv`。
10. 提供 `SetPlannerAlgorithm.srv`。
11. 支持配置切换 dijkstra/astar、reference_line/stop_and_wait/frenet_lite/hybrid_astar_parking、constant/curvature/obstacle_aware。
12. 如果 AD Package validation failed，node 进入错误状态并拒绝规划。
13. 如果没有 route，发布 failure status 和安全 stop trajectory。

生成：

```text
src/low_speed_av_planning/config/planning_params.yaml
src/low_speed_av_planning/launch/planning.launch.py
```

创建：

```text
reports/phase_05_report.md
```

不要声称已 colcon build。
