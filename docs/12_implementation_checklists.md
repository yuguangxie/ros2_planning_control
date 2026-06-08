# 12 Implementation Checklists

## AD Package Compatibility Checklist

- [ ] Reads `project_manifest.json`.
- [ ] Does not require `manifest.json`.
- [ ] Reads `trajectory/waypoints.yaml`.
- [ ] Does not require `trajectory/waypoints.json`.
- [ ] Reads `validation/validation_report.json`.
- [ ] Rejects failed validation.
- [ ] Supports manifest `files` lookup.
- [ ] Supports checksums.
- [ ] Supports waypoint fields `x/y/yaw/kappa/v_mps/s_m`.
- [ ] Supports legacy `end_index` and preferred `end_index_exclusive`.
- [ ] Loads task/parking/charging semantics.

## Planning Checklist

- [ ] Dijkstra implemented.
- [ ] A* implemented.
- [ ] Algorithm selection through config.
- [ ] Runtime blocked edges supported.
- [ ] Reverse edges/gear hints preserved.
- [ ] Waypoint stitching works.
- [ ] Route-level `s_m` regenerated.
- [ ] Curvature speed planning works.
- [ ] Stop trajectory available.
- [ ] Status messages published.

## Control Checklist

- [ ] Configurable localization topic, default `/localization/pose`.
- [ ] Pure Pursuit implemented.
- [ ] Stanley implemented.
- [ ] Riccati-based LQR implemented with curvature feedforward.
- [ ] MPC sampler implemented.
- [ ] Front Ackermann model implemented.
- [ ] Dual Ackermann model implemented.
- [ ] Command smoother implemented.
- [ ] Localization timeout triggers stop.
- [ ] Trajectory timeout triggers stop.
- [ ] Output includes front and rear steering.
- [ ] SCU command output publishes `/yunle_chassis/control/scu_control_command`.
- [ ] SCU shift output is always one of D=1, N=2, R=3.
- [ ] SCU speed is non-negative km/h and steering is degrees.

## No-ROS2 Acceptance Checklist

- [ ] `reports/phase_xx_report.md` exists for every phase.
- [ ] Offline AD Package validation script passes.
- [ ] Offline algorithm smoke script passes.
- [ ] Final report lists ROS2 commands as not run.
- [ ] No claim of successful `colcon build` in Codex environment.
