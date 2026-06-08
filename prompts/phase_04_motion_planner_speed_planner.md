# Phase 04 — Implement Motion Planner and Speed Planner

请实现局部 motion planner 和 speed planner。

Motion planner 算法可选：

```text
reference_line
stop_and_wait
frenet_lite
hybrid_astar_parking
```

最低要求：

1. `reference_line` 完整实现。
2. `stop_and_wait` 输出安全停车轨迹。
3. `frenet_lite` 和 `hybrid_astar_parking` 先实现可替换 skeleton，接口完整，报告 TODO。
4. 使用 `waypoint_index + waypoints.yaml` 按 global planner 输出的 edge sequence 拼接 trajectory。
5. 去除相邻 edge 的重复点。
6. 重新生成 route-level `s_m`。
7. 根据当前 pose 找最近点。
8. 按 `horizon_distance_m` 裁剪局部轨迹。
9. 保留 `edge_id/waypoint_id/path_id/behavior/gear`。

Speed planner 算法可选：

```text
constant
curvature
obstacle_aware
```

最低要求：

- constant 完整实现。
- curvature 根据 `kappa` 和最大横向加速度限速。
- obstacle_aware 可以是保守 stub。

更新 offline smoke，使其验证可以由 edge sequence 拼接 trajectory。

创建：

```text
reports/phase_04_report.md
```
