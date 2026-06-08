# 规划模块审计 3

## Objective（目标）
审计 planning 包的 RoadnetLoader、Dijkstra/A*、motion/speed planner factory、planning 服务、route/trajectory/status 发布和失败安全行为。

## Status（状态）
Partial。源码层面 planning pipeline 基本可用，离线 Python smoke 能生成 sample route 和 trajectory；但 ROS2 service callback 的真实运行、publisher 行为和参数加载未在 ROS2 环境验证。

## Evidence（证据）
- 参数声明包括 roadnet、topics、planner 选择：`src/low_speed_av_planning/src/planning_node.cpp:45` 到 `src/low_speed_av_planning/src/planning_node.cpp:66`。
- route/trajectory/status 发布器：`src/low_speed_av_planning/src/planning_node.cpp:72` 到 `src/low_speed_av_planning/src/planning_node.cpp:79`。
- ReloadRoadnet、PlanRoute、SetPlannerAlgorithm 服务：`src/low_speed_av_planning/src/planning_node.cpp:81` 到 `src/low_speed_av_planning/src/planning_node.cpp:99`。
- 加载 roadnet package：`src/low_speed_av_planning/src/planning_node.cpp:108` 到 `src/low_speed_av_planning/src/planning_node.cpp:118`。
- 失败安全 stop trajectory：`src/low_speed_av_planning/src/planning_node.cpp:159` 到 `src/low_speed_av_planning/src/planning_node.cpp:188`。
- blocked_edges 合并配置和语义：`src/low_speed_av_planning/src/planning_node.cpp:194` 到 `src/low_speed_av_planning/src/planning_node.cpp:200`。
- motion/speed 组合：`src/low_speed_av_planning/src/planning_node.cpp:239` 到 `src/low_speed_av_planning/src/planning_node.cpp:248`。
- PlanRoute callback 发布 route/trajectory：`src/low_speed_av_planning/src/planning_node.cpp:386` 到 `src/low_speed_av_planning/src/planning_node.cpp:428`。
- Dijkstra/A* factory：`src/low_speed_av_planning/src/global_planner_factory.cpp:12` 到 `src/low_speed_av_planning/src/global_planner_factory.cpp:15`。
- motion factory 算法列表：`src/low_speed_av_planning/src/reference_line_motion_planner.cpp:75` 到 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:84`。
- speed factory 算法列表：`src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:23` 到 `src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:29`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-PL-001 | P3 | Pass | planning 服务和发布器已实现，不再只是 skeleton。 |
| A3-PL-002 | P3 | Pass | Dijkstra/A* 均尊重 blocked_edges 和 allow_reverse。 |
| A3-PL-003 | P3 | Pass | `reference_line` 能基于 waypoint_index 拼接轨迹，`stop_and_wait` 输出停止轨迹。 |
| A3-PL-004 | P2 | Partial | `frenet_lite` 与 `hybrid_astar_parking` 仍是 fallback/skeleton 算法，应避免作为生产默认。 |
| A3-PL-005 | P2 | Partial | planning pipeline 通过 Python smoke 验证，但服务调用和 topic 发布没有 ROS2 runtime 证据。 |
| A3-PL-006 | P2 | Partial | no_go 约束已影响 blocked_edges，但几何检测精度不足。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
默认 `astar + reference_line + curvature` 路径具备可用源码链路。若选择 skeleton motion planner，系统不会输出高速轨迹，但路径能力有限。ROS2 未验证意味着实际服务参数、QoS 和 topic 类型仍可能存在集成问题。

## Recommended fix（推荐修复）
- 添加 C++ CLI smoke 或 gtest 直接调用 `GlobalPlannerFactory`、`MotionPlannerFactory`、`SpeedPlannerFactory`。
- 在 ROS2 环境调用 `/low_speed_av_planning/plan_route` 并 echo `/planning/global_route`、`/planning/trajectory`。
- 文档中明确 `frenet_lite`、`hybrid_astar_parking` 为 experimental/fallback。

## Verification method（验证方法）
- 已运行 `offline_algorithm_smoke.py`，结果为 route `['E_L001_F', 'E_L002_F']`，trajectory 6 点。
- 已运行 `offline_remaining_fixes_smoke.py`，确认 semantics 影响 route/speed。
- 未运行 ROS2 service/topic。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 service call /low_speed_av_planning/plan_route low_speed_av_interfaces/srv/PlanRoute ...`
- `ros2 topic echo /planning/global_route`
- `ros2 topic echo /planning/trajectory`
- `ros2 topic echo /planning/status`
