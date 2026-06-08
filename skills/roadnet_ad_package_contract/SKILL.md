# Skill: Roadnet AD Package v1.1 Contract

Use this skill whenever implementing `RoadnetLoader`, test fixtures, map/roadnet validators, or planner input conversion.

## Canonical Entry

The canonical entry is:

```text
project_manifest.json
```

Do not hard-code internal file paths unless `manifest.files` misses a key. Preferred path resolution:

1. Load `project_manifest.json`.
2. Check `schema == "low_speed_roadnet_ad_package"`.
3. Check `schema_version` starts with `1.1.` or equals `1.1.0`.
4. Check validation summary in manifest.
5. Resolve file paths through `manifest.files`.
6. Verify `checksums.sha256` or `manifest.hashes` when present.
7. Load topology, waypoints, waypoint_index, semantics, validation.

## Required Files

```text
project_manifest.json
checksums.sha256
map/map_metadata.yaml
roadnet/roadnet.json
roadnet/topology.json
roadnet/route_graph.yaml
trajectory/waypoints.yaml
trajectory/waypoints.csv
trajectory/waypoint_index.json
semantics/areas.json
semantics/route_points.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
schemas/project_manifest.schema.json
schemas/roadnet.schema.json
schemas/topology.schema.json
schemas/waypoints.schema.json
schemas/waypoint_index.schema.json
schemas/semantics.schema.json
schemas/validation_report.schema.json
examples/mission.example.json
```

## Topology

`roadnet/topology.json` is the main global planning graph.

Required top-level fields:

```json
{
  "schema": "low_speed_topology",
  "schema_version": "1.1.0",
  "coordinate_frame": "map",
  "nodes": [],
  "edges": []
}
```

Each node must provide at least `id` and `pose.x/y/yaw` or equivalent fields.

Each edge should provide:

```json
{
  "id": "E_L-001_F",
  "from": "N0001",
  "to": "N0002",
  "direction": "forward",
  "length_m": 5.0,
  "cost": 6.25,
  "speed_limit_mps": 0.8,
  "constraints": {},
  "waypoint_range": {"start_index": 0, "end_index": 25},
  "reference_points": []
}
```

The loader must support both:

```text
legacy: start_index + end_index, where end_index is inclusive
preferred: start_index + end_index_exclusive + count
```

## Waypoints

`trajectory/waypoints.yaml` is the primary trajectory data file.

Required waypoint fields:

```yaml
global_index: 0
waypoint_id: WP_E_L-001_F_000000
edge_id: E_L-001_F
path_id: L-001
node_from: N0001
node_to: N0002
s_m: 0.0
x: 0.0
y: 0.0
yaw: 0.0
kappa: 0.0
v_mps: 0.8
speed_limit_mps: 0.8
behavior: follow
direction: forward
flags: [edge_start]
```

Internal field mapping:

```text
x -> x_m
y -> y_m
yaw -> yaw_rad
kappa -> kappa_1pm
v_mps -> target_speed_mps
s_m -> edge_s_m
```

When concatenating multiple edges, regenerate route-level `s_m` and `relative_time_s`.

## Validation

`validation/validation_report.json` and manifest `validation` both matter. Reject package when:

```text
validation.status == "failed"
blocking_errors > 0
```

Allow `warning`, but publish degraded/warning status.

## Semantics

Use:

- `semantics/areas.json` for drivable/no-go/speed-zone constraints.
- `semantics/task_points.json` for mission goals.
- `semantics/parking_points.json` for parking goals.
- `semantics/charging_points.json` for charging goals.

If task/parking/charging point lacks `linked_s_m`, the loader may project to nearest waypoint and report a warning. Prefer export-side projection if implementing exporter.
