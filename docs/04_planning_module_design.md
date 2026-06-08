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
