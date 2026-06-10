# Planning Module Function Audit

## Objective

审计 planning 模块的 RoadnetLoader、全局规划、运动规划、速度规划、服务、publisher 和失败行为。

## Scope

- `src/low_speed_av_planning`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_planning/config/planning_params.yaml`

## Status

Partial。静态实现较完整，ROS2 runtime 未验证。

## Evidence

- 参数声明：`src/low_speed_av_planning/src/planning_node.cpp:75` 到 `src/low_speed_av_planning/src/planning_node.cpp:102`。
- Publishers：`src/low_speed_av_planning/src/planning_node.cpp:108` 到 `src/low_speed_av_planning/src/planning_node.cpp:114`。
- Services：`src/low_speed_av_planning/src/planning_node.cpp:122` 到 `src/low_speed_av_planning/src/planning_node.cpp:136`。
- Roadnet 空路径等待：`src/low_speed_av_planning/src/planning_node.cpp:151`。
- `compute_route` 检查 roadnet 和 node：`src/low_speed_av_planning/src/planning_node.cpp:267` 到 `src/low_speed_av_planning/src/planning_node.cpp:277`。
- `compute_trajectory` 调用 motion/speed planner：`src/low_speed_av_planning/src/planning_node.cpp:280` 到 `src/low_speed_av_planning/src/planning_node.cpp:291`。
- 失败轨迹：`src/low_speed_av_planning/src/planning_node.cpp:200`。
- PlanRoute 主流程：`src/low_speed_av_planning/src/planning_node.cpp:560` 到 `src/low_speed_av_planning/src/planning_node.cpp:608`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-PLAN-001 | P3 | Pass | ReloadRoadnet、PlanRoute、SetPlannerAlgorithm 均创建服务。 | 具备运行时换图和规划入口。 | 无。 | `ros2 service list`。 |
| AUD5-PLAN-002 | P3 | Pass | 成功路径发布 `/planning/global_route` 和 `/planning/trajectory`。 | 控制模块可继续消费 trajectory。 | 无。 | PlanRoute 后 echo 两个 topic。 |
| AUD5-PLAN-003 | P2 | Partial | 失败轨迹使用 package 第一个 waypoint，而不是当前 pose。 | 规划失败时 RViz/控制看到的 stop point 可能不在车辆当前位置。 | 后续可用 latest pose 构造 failure stop trajectory。 | 缺路网/坏 goal 场景验证。 |
| AUD5-PLAN-004 | P2 | Partial | no-go 阻断依赖 waypoint 是否落入区域，边线穿越但采样点未覆盖时可能漏判。 | 语义约束保守性不足。 | 对 edge reference segment 做线段-多边形相交检查。 | 构造穿越 no-go 的边测试。 |
| AUD5-PLAN-005 | P1 | Not Verified | 未在 ROS2 环境实际调用 PlanRoute。 | 服务 QoS、参数、launch 可能有集成问题。 | 执行人工 ROS2 验证 I/J。 | `ros2 service call /low_speed_av_planning/plan_route ...`。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package`
- `uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 service call /low_speed_av_planning/plan_route ...`
- `ros2 topic echo /planning/trajectory`

## Remaining Uncertainty

算法离线 smoke 通过，但 node 级服务调用和 topic 发布需在 ROS2 环境验证。

