# Skill: Testing and Acceptance Without ROS2

Use this skill because the Codex environment does not have ROS2.

## Do Not Require

Do not require:

```bash
colcon build
colcon test
ros2 launch
ros2 topic
```

unless ROS2 is detected.

## Required Offline Checks

Generate scripts under `scripts/` or `low_speed_av_bringup/scripts/`:

```text
validate_expected_tree.py
validate_sample_ad_package.py
offline_algorithm_smoke.py
```

They should run with Python standard library plus PyYAML if available. If PyYAML is missing, either parse simple YAML conservatively or explain skip.

## validate_expected_tree.py

Checks required package folders and key files exist.

## validate_sample_ad_package.py

Checks:

- `project_manifest.json` exists.
- schema is `low_speed_roadnet_ad_package`.
- schema version is `1.1.0` or compatible.
- required files exist.
- validation not failed.
- topology nodes and edges are consistent.
- waypoint_index ranges are in bounds.
- waypoints have `x/y/yaw/kappa/v_mps/s_m/edge_id/path_id`.
- checksums match when present.

## offline_algorithm_smoke.py

Pure Python reference check:

- load sample topology,
- run Dijkstra and A* from first to last node,
- slice waypoints by edge sequence,
- generate local horizon,
- compute simple pure pursuit and Stanley steering values,
- verify command is finite and within configured limit.

## Phase Report

Every phase report must include skipped ROS2 commands:

```text
- colcon build: SKIPPED_ROS2_UNAVAILABLE
- colcon test: SKIPPED_ROS2_UNAVAILABLE
```

This is acceptable in Codex. Real ROS2 validation happens later on target machine.
