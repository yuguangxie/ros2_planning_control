# Phase 03 — Implement Global Planner Algorithms

请在 `low_speed_av_planning` 中实现全局规划算法：

```text
dijkstra
astar
```

要求：

1. 从 RoadnetLoader 输出的 topology nodes/edges 构建 adjacency list。
2. 使用 edge.cost。
3. 支持 allow_reverse 配置。
4. 支持 blocked_edges 配置。
5. 支持 edge validation/availability 过滤；如果字段不存在，默认可用。
6. A* 使用节点 pose 的欧氏距离作为 heuristic。
7. 输出 edge_id sequence、node_id sequence、length_m、estimated_time_s、status/message。
8. 提供工厂：`GlobalPlannerFactory`。
9. 不依赖 ROS2 node 即可单元测试核心算法。

生成或更新离线脚本：

```text
scripts/offline_algorithm_smoke.py
```

至少验证 sample AD Package 中 N0001 -> N0003 可以得到非空 edge 序列。

创建：

```text
reports/phase_03_report.md
```
