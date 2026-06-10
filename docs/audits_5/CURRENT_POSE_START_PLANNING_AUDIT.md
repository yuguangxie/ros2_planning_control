# Current Pose Start Planning Audit

## Objective

审计规划节点使用当前 `/localization/pose` 推断路线起点的新增行为是否实现、兼容且可验证。

## Scope

- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- planning YAML
- `scripts/offline_simulation_smoke.py`

## Status

Partial。静态实现和离线 smoke 通过，ROS2 service/topic 行为未验证。

## Evidence

- planning 参数：`src/low_speed_av_planning/src/planning_node.cpp:98` 到 `src/low_speed_av_planning/src/planning_node.cpp:102`。
- PoseStamped 订阅：`src/low_speed_av_planning/src/planning_node.cpp:116` 到 `src/low_speed_av_planning/src/planning_node.cpp:119`。
- pose 回调与有限值检查：`src/low_speed_av_planning/src/planning_node.cpp:293`。
- start 解析：`src/low_speed_av_planning/src/planning_node.cpp:330`。
- current-pose matcher：`src/low_speed_av_planning/src/planning_node.cpp:348`。
- PlanRoute 使用 `resolve_start_node`：`src/low_speed_av_planning/src/planning_node.cpp:565`。
- 离线 simulation smoke 输出匹配 `N0001` 并生成 `N0001 -> N0003` 路线。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-CP-001 | P3 | Pass | 显式 start 字段优先，旧行为保持。 | 旧客户端兼容。 | 无。 | 显式 `N0001 -> N0003` PlanRoute。 |
| AUD5-CP-002 | P3 | Pass | 空 start 且参数启用时使用最新 pose。 | 支持实用起点规划。 | 无。 | 空 start PlanRoute。 |
| AUD5-CP-003 | P2 | Partial | matcher 使用最近 waypoint，而非连续 edge projection。 | pose 在曲线/交叉口/边中段时可能选择不理想 start node。 | 增加投影距离、方向一致性、剩余 edge prefix 处理。 | 构造多边交叉口和中段 pose。 |
| AUD5-CP-004 | P2 | Partial | `planning.start_match_prefer_edge_projection` 的命名比实际逻辑更强；当前按 edge 内 progress 推断 node，不是真投影。 | 参数名可能让使用者误解。 | 文档已说明；后续可重命名或实现真投影。 | 审查文档和代码。 |
| AUD5-CP-005 | P1 | Not Verified | 未在 ROS2 中验证 stale pose、missing pose、far pose 的服务响应。 | 失败路径可能影响任务系统行为。 | 人工验证 K 节。 | 暂停定位、移动远处 pose 后调用服务。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 topic echo /localization/pose`
- `ros2 service call /low_speed_av_planning/plan_route ... start_node_id: ''`

## Remaining Uncertainty

未验证 ROS2 时间戳、QoS、实际 pose 到 matcher 的延迟与 node lifecycle。

