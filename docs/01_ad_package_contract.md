# 01 AD Package v1.1 Contract Alignment

## Canonical ZIP Structure

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

## Manifest

`project_manifest.json` is the only recommended entry.

Essential fields:

```json
{
  "schema": "low_speed_roadnet_ad_package",
  "schema_version": "1.1.0",
  "package_id": "pkg_demo_20260608",
  "coordinate_system": {
    "global_frame": "map",
    "control_reference_frame": "rear_axle",
    "angle_unit": "rad",
    "distance_unit": "m"
  },
  "units": {
    "distance": "m",
    "angle": "rad",
    "speed": "mps",
    "curvature": "1/m"
  },
  "files": {
    "topology": "roadnet/topology.json",
    "waypoints_yaml": "trajectory/waypoints.yaml",
    "waypoint_index": "trajectory/waypoint_index.json",
    "areas": "semantics/areas.json",
    "task_points": "semantics/task_points.json",
    "parking_points": "semantics/parking_points.json",
    "charging_points": "semantics/charging_points.json",
    "validation_report": "validation/validation_report.json"
  },
  "validation": {
    "status": "passed",
    "blocking_errors": 0,
    "warnings": 0
  }
}
```

## Loader Acceptance Rules

Planning loader must reject:

```text
missing project_manifest.json
schema != low_speed_roadnet_ad_package
unsupported schema_version
validation.status == failed
validation.blocking_errors > 0
missing topology
missing waypoints_yaml
missing waypoint_index
```

Planning loader may warn but continue:

```text
validation.status == warning
missing optional semantics file
manifest.files incomplete but canonical fallback path exists
legacy waypoint_index end_index instead of end_index_exclusive
missing linked_s_m in task/parking/charging point; projection fallback is possible
```

## Topology Use

`roadnet/topology.json` drives global planning. Edges must provide:

```text
id, from, to, cost, direction, length_m, speed_limit_mps, waypoint_range/reference_points
```

## Waypoints Use

`trajectory/waypoints.yaml` drives motion planning and controller reference creation.

Canonical field mapping:

| Package field | Internal field | Meaning |
|---|---|---|
| `x` | `x_m` | map x |
| `y` | `y_m` | map y |
| `yaw` | `yaw_rad` | heading |
| `kappa` | `kappa_1pm` | curvature |
| `v_mps` | `target_speed_mps` | target speed |
| `s_m` | `edge_s_m` | distance within edge |
| `edge_id` | `edge_id` | topology edge |
| `path_id` | `path_id` | editor path |
| `direction` | `gear_hint` | forward/reverse |

After stitching multiple edges, regenerate route-level `s_m`.

## Semantics Use

- `areas.json`: speed zones, drivable zones, no-go zones.
- `task_points.json`: mission goals.
- `parking_points.json`: parking goals.
- `charging_points.json`: charging goals.

## Compatibility With Older Prompt Pack

The older pack may mention:

```text
manifest.json
trajectory/waypoints.json
validation_report.json
```

These are obsolete for this project. The current implementation must use:

```text
project_manifest.json
trajectory/waypoints.yaml
validation/validation_report.json
```

## Phase 15 路径与结构安全规则

所有来自 `manifest.files`、`manifest.hashes` 和 `checksums.sha256` 的路径先统一反斜杠语义，再相对 canonical package root 解析。实现拒绝 POSIX/Windows 绝对路径、UNC、任意 `..` 分量，以及 canonical 后位于 root 外的 symlink 目标。Checksum 与最终文件读取使用同一 containment helper。

Loader 还会 fail closed 拒绝重复 node/edge/waypoint/semantic ID、未知 node/edge 引用、负数或非有限 cost/length/speed/waypoint 数值、非法或重叠 waypoint range、count 不一致、range 内 edge_id 不一致和未被 index 覆盖的 waypoint。错误消息包含对应 ID、index 或字段。
