# Phase 10 — Tests and Acceptance Without ROS2

当前环境没有 ROS2，请不要把 `colcon build` 作为必须成功项。

请生成并运行可运行的离线脚本：

```text
scripts/validate_expected_tree.py
scripts/validate_sample_ad_package.py
scripts/offline_algorithm_smoke.py
```

脚本应验证：

1. workspace 四包结构完整。
2. interfaces 文件存在。
3. planning/control 关键源码存在。
4. sample AD Package 文件清单完整。
5. `project_manifest.json` schema/version 正确。
6. `validation/validation_report.json` passed。
7. topology node/edge 引用合法。
8. waypoints.yaml 字段齐全。
9. waypoint_index 切片合法。
10. Dijkstra 和 A* 能产生 edge sequence。
11. trajectory stitching 能产生局部轨迹。
12. Pure Pursuit/Stanley offline 计算输出有限值。

创建：

```text
reports/phase_10_report.md
```

报告中写明：

```text
colcon build: SKIPPED_ROS2_UNAVAILABLE
colcon test: SKIPPED_ROS2_UNAVAILABLE
```
