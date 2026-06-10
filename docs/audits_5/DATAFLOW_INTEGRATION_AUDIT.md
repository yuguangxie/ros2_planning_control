# Dataflow Integration Audit

## Objective

审计从 AD Package 到可视化、规划、控制和 Yunle SCU 输出的端到端数据流是否完整。

## Scope

全模块端到端集成路径。

## Status

Partial。静态链路完整，ROS2 runtime 未验证。

## Evidence

```text
roadnet_ad_package_20260610T012525Z
  -> RoadnetLoader
  -> planning graph + waypoint index + semantics
  -> roadnet visualization markers
  -> simulated /localization/pose
  -> current-pose start matcher
  -> PlanRoute goal request
  -> /planning/global_route
  -> /planning/trajectory
  -> control node
  -> controller algorithm
  -> vehicle model
  -> limiter/smoother
  -> SCU mapper
  -> /yunle_chassis/control/scu_control_command
```

关键证据：

- RoadnetLoader 被 planning 和 simulation 复用：`src/low_speed_av_simulation/CMakeLists.txt:15` 到 `src/low_speed_av_simulation/CMakeLists.txt:16`。
- Planning 输出 route/trajectory：`src/low_speed_av_planning/src/planning_node.cpp:108` 到 `src/low_speed_av_planning/src/planning_node.cpp:110`。
- Control 订阅 trajectory：`src/low_speed_av_control/src/control_node.cpp:86`。
- Control 发布 SCU：`src/low_speed_av_control/src/control_node.cpp:97`。
- Visualization 订阅 route/trajectory/pose：`src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:107` 到 `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:119`。

## Topic/Service Table

| Producer | Topic/Service | Type | Consumer | Purpose | Simulation required | Real vehicle required |
|---|---|---|---|---|---|---|
| planning | `/low_speed_av_planning/reload_roadnet` | `low_speed_av_interfaces/srv/ReloadRoadnet` | operator/task | load AD Package | optional | yes for map switching |
| planning | `/low_speed_av_planning/plan_route` | `low_speed_av_interfaces/srv/PlanRoute` | operator/task | trigger route | yes | yes |
| sim localization | `/localization/pose` | `geometry_msgs/msg/PoseStamped` | planning/control/visualization | current pose | yes | from real localization |
| planning | `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | visualization/operator | route debug | yes | optional |
| planning | `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | control/visualization | control reference | yes | yes |
| planning | `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | operator | planning health | yes | yes |
| planning | `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | operator | roadnet readiness | yes | yes |
| simulation | `/simulation/roadnet_markers` | `visualization_msgs/msg/MarkerArray` | RViz | base map display | yes | optional |
| simulation | `/simulation/route_markers` | `visualization_msgs/msg/MarkerArray` | RViz | planned route display | yes | optional |
| simulation | `/simulation/trajectory_path` | `nav_msgs/msg/Path` | RViz | trajectory display | yes | optional |
| simulation | `/simulation/vehicle_markers` | `visualization_msgs/msg/MarkerArray` | RViz | vehicle pose display | yes | optional |
| simulation | `/simulation/start|pause|reset` | `std_srvs/srv/Trigger` | operator | pose playback control | yes | no |
| control | `/control/command` | `low_speed_av_interfaces/msg/ControlCommand` | debug | internal command | optional | optional |
| control | `/control/status` | `low_speed_av_interfaces/msg/ModuleStatus` | operator | control health | yes | yes |
| control | `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | chassis driver | final command | bench only | yes |

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-DF-001 | P3 | Pass | Topic contracts between planning and control remain unchanged. | control can consume planned trajectory. | 无。 | PlanRoute 后 echo control status/SCU。 |
| AUD5-DF-002 | P2 | Partial | Simulation can publish `/localization/pose` on same topic as real localization. | 若误连真实车辆，可能造成控制输出。 | 文档警告已写；后续建议 namespace 或 safety gate。 | launch 参数审查和 bench 测试。 |
| AUD5-DF-003 | P1 | Not Verified | Full dataflow 未在 ROS2 中跑通。 | 实际集成风险仍存在。 | 逐步执行人工验证流程。 | 本目录 ROS2_MANUAL_VALIDATION_PROCEDURE。 |

## ROS2 Commands Run Or Skipped

Run:

- Offline scripts listed in summary.

SKIPPED_ROS2_UNAVAILABLE:

- Full launch and topic/service validation.

## Remaining Uncertainty

端到端频率、QoS、时间戳、frame_id 和真实底盘响应未验证。

