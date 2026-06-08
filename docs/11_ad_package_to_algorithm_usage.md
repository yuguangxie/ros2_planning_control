# 11 AD Package to Planning and Control Algorithm Usage

## Global Planning Example

Input files:

```text
project_manifest.json
roadnet/topology.json
validation/validation_report.json
```

Pseudo flow:

```python
manifest = load_json("project_manifest.json")
assert manifest["schema"] == "low_speed_roadnet_ad_package"
assert manifest["schema_version"].startswith("1.1")
assert manifest["validation"]["status"] != "failed"

topology = load_json(manifest["files"]["topology"])
graph = build_adjacency(topology["nodes"], topology["edges"])
edge_ids = astar(graph, start_node_id="N0001", goal_node_id="N0003")
```

## Motion Planning Example

Input files:

```text
trajectory/waypoints.yaml
trajectory/waypoint_index.json
```

Pseudo flow:

```python
waypoints = load_yaml("trajectory/waypoints.yaml")["waypoints"]
index = load_json("trajectory/waypoint_index.json")
trajectory = []
for edge_id in edge_ids:
    rng = index["edges"][edge_id]
    start = rng["start_index"]
    end_exclusive = rng.get("end_index_exclusive", rng["end_index"] + 1)
    part = waypoints[start:end_exclusive]
    if trajectory and part and trajectory[-1]["waypoint_id"] == part[0]["waypoint_id"]:
        part = part[1:]
    trajectory.extend(part)
trajectory = regenerate_route_s(trajectory)
local = crop_by_pose_and_horizon(trajectory, pose, horizon_m=15.0)
```

## Pure Pursuit Usage

Required fields:

```text
x, y, yaw, v_mps, s_m
```

Algorithm:

```text
nearest = find_nearest(local_trajectory, pose)
target = find_by_s(local_trajectory, nearest.s_m + lookahead)
target_vehicle = transform_map_to_vehicle(target, pose)
kappa_cmd = 2 * target_vehicle.y / lookahead^2
front_steer, rear_steer = vehicle_model.curvature_to_steering(kappa_cmd)
speed = target.v_mps
```

## Stanley Usage

Required fields:

```text
x, y, yaw, v_mps
```

```text
nearest = find_nearest(local_trajectory, pose)
heading_error = normalize(nearest.yaw - pose.yaw)
cross_track_error = signed_lateral_error(pose, nearest)
steer = heading_error + atan2(k * cross_track_error, abs(speed) + epsilon)
```

## LQR Usage

Required fields:

```text
x, y, yaw, kappa_1pm, v_mps, s_m
```

Use nearest reference plus configurable preview. The controller solves a two-state discrete Riccati equation for `[e_y, e_psi]`, adds curvature feedforward `atan(wheel_base * kappa_ref)`, and outputs `desired_curvature_1pm`. The selected vehicle model converts that curvature to front/rear steering.

## MPC Sampler Usage

Required fields:

```text
x, y, yaw, kappa, v_mps, s_m
```

Use horizon points. Sample curvature commands and roll out low-speed kinematic model. Pick candidate with minimum lateral, heading, speed and smoothness cost.
