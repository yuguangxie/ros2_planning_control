# Skill: Low-Speed Planning Algorithms

Use this skill to implement global and local planning based on the custom roadnet package.

## Global Planning

Inputs:

- current pose or start node/task point,
- goal node/task/parking/charging point,
- `roadnet/topology.json`,
- optional runtime blocked edge list,
- optional semantic constraints.

Algorithms:

```text
dijkstra
astar
```

Dijkstra uses `edge.cost`. A* uses `edge.cost + heuristic`, where heuristic is Euclidean distance between node poses divided by nominal speed or multiplied by cost scale. A* must never overestimate if strict optimality is required.

Edge filtering:

```text
skip if edge availability disabled
skip if edge violates vehicle max curvature
skip if edge intersects no_go_area when precomputed validation says invalid
skip if edge is runtime-blocked
skip if direction/gear not allowed by mission
```

Output:

```text
edge_id sequence
node_id sequence
length_m
estimated_time_s
status
```

## Motion Planning

Main algorithm: `reference_line`.

Steps:

1. Receive edge sequence from global planner.
2. Use `waypoint_index.edges[edge_id]` to slice `waypoints.yaml`.
3. Remove duplicate boundary points between consecutive edges.
4. Regenerate continuous `route_s_m`.
5. Find nearest point to current `/localization/pose`.
6. Crop local horizon by distance or time.
7. Smooth optional, but do not move points outside `drivable_area`.
8. Apply speed planner.
9. Publish `Trajectory`.

Other algorithms:

- `stop_and_wait`: output hold/stop trajectory.
- `frenet_lite`: stub structure for lateral offsets and simple candidates.
- `hybrid_astar_parking`: stub interface for parking/narrow reverse maneuvers.

## Speed Planning

Algorithms:

```text
constant
curvature
obstacle_aware
```

Curvature speed rule example:

```text
v = min(edge_speed_limit, sqrt(max_lateral_accel / max(abs(kappa), epsilon)))
```

Obstacle-aware can initially be conservative: if obstacle distance along path is below threshold, ramp speed to zero.

## Output Trajectory Fields

Each trajectory point should have at least:

```text
x_m, y_m, yaw_rad, kappa_1pm, s_m, v_mps, a_mps2, relative_time_s, gear, behavior, edge_id, waypoint_id
```

Do not publish UI control points to the controller. Controller consumes trajectory only.
