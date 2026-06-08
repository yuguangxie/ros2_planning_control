# Goal 01 — Generate Planning Module Only

请只生成或修改：

```text
src/low_speed_av_interfaces
src/low_speed_av_planning
src/low_speed_av_bringup 中与 planning 有关的 config/launch/sample
scripts 中 AD Package 和 planning 离线检查
```

不要实现控制算法。控制相关只保留接口消息。

必须以当前 AD Package v1.1 为准：

```text
project_manifest.json
trajectory/waypoints.yaml
validation/validation_report.json
```

实现 RoadnetLoader、Dijkstra、A*、reference_line motion planner、constant/curvature speed planner、planning node、planning params、planning launch、离线测试和阶段报告。

当前环境没有 ROS2，不要求运行 colcon。运行纯 Python 离线检查并写报告。
