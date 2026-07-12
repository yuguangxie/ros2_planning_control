# 04 Planning Module Design

## Node

```text
low_speed_av_planning_node
```

## Main Classes

```text
RoadnetLoader
RoadnetMap
TopologyGraph
WaypointStore
SemanticMap
GlobalPlannerBase
DijkstraPlanner
AStarPlanner
MotionPlannerBase
ReferenceLineMotionPlanner
StopAndWaitMotionPlanner
FrenetLiteMotionPlanner
HybridAStarParkingPlannerStub
SpeedPlannerBase
ConstantSpeedPlanner
CurvatureSpeedPlanner
ObstacleAwareSpeedPlanner
PlanningNode
```

## RoadnetLoader

Inputs:

```yaml
roadnet:
  package_path: ""
  allow_validation_warning: true
  reject_failed_validation: true
  verify_checksums: true
```

Output object:

```text
RoadnetMap
  package_id
  schema_version
  coordinate_system
  units
  map_metadata
  topology.nodes
  topology.edges
  waypoint_store.waypoints
  waypoint_index.edges
  semantics.areas/task_points/parking_points/charging_points
  validation_report
```

## Global Planner

Interface:

```cpp
class GlobalPlannerBase {
 public:
  virtual PlanningResult plan(const RoadnetMap&, const PlanningQuery&) = 0;
};
```

Dijkstra:

```text
priority_queue by accumulated cost
skip unavailable/blocked/invalid edges
reconstruct edge sequence when goal node reached
```

A*:

```text
priority = g_cost + heuristic_weight * EuclideanDistance(current, goal)
```

## Motion Planner

Main reference_line algorithm:

```text
edge_ids -> waypoint slices -> deduplicate -> route_s_m -> crop horizon by current pose -> speed planner -> trajectory
```

The planner must not pass the full global waypoints to control if the vehicle only needs local horizon.

## Speed Planner

Curvature speed:

```text
v_curv = sqrt(max_lateral_accel_mps2 / max(abs(kappa), min_curvature_epsilon))
v = min(v_curv, speed_limit_mps, configured_max_speed_mps)
```

Obstacle-aware stub:

- Subscribe optional obstacle topic later.
- In phase 1, accept runtime obstacle distance through an internal optional API or config.
- If obstacle distance is below stop distance, output stop profile.

## Planning State Machine

```text
UNCONFIGURED
LOADING_ROADNET
ROADNET_READY
WAITING_MISSION
PLANNING_GLOBAL
PLANNING_MOTION
PUBLISHING_TRAJECTORY
BLOCKED
ERROR
EMERGENCY_STOP
```

## Failure Modes

- Missing AD Package.
- AD Package validation failed.
- No path between nodes.
- Empty waypoint sequence.
- Localization timeout.
- Goal not on topology.

In failures, publish status and safe stop trajectory if appropriate.

## Phase 15 图搜索与 helper 合同

Edge cost 必须有限且不小于零，Loader 与 planner 两层都 fail closed。零 cost 允许，但会使 admissible heuristic scale 降为零，A* 安全退化为 Dijkstra。默认 `heuristic_weight <= 1` 时，启发式为直线距离乘以全图最小 `edge.cost / endpoint_distance`；大于 1 时明确报告 weighted A*，不承诺最优。

邻接边按 `(to_node_id, edge_id)` 排序，open queue 与等价 parent 更新使用稳定 ID tie-break。`planning_helpers` 位于 production target 内，负责 node/semantic/current-pose anchor、linked-edge fallback、同 edge terminal segment、route s、GlobalRoute 几何摘要、连续性、semantic speed 和有状态 local crop。Progress tracker 以 trajectory identity 隔离路线，并在有限窗口内结合 heading 搜索，禁止回环处无约束全局最近点跳进度。
