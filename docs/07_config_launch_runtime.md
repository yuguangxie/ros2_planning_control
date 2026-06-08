# 07 Config, Launch and Runtime

## planning_params.yaml

```yaml
low_speed_av_planning:
  ros__parameters:
    roadnet:
      package_path: ""
      reject_failed_validation: true
      allow_validation_warning: true
      verify_checksums: true
    topics:
      localization_pose_topic: "/localization/pose"
      localization_pose_type: "pose_stamped"
      global_route_topic: "/planning/global_route"
      trajectory_topic: "/planning/trajectory"
      planning_status_topic: "/planning/status"
      roadnet_status_topic: "/planning/roadnet_status"
    global_planner:
      algorithm: "astar"
      heuristic_weight: 1.0
      allow_reverse: true
      blocked_edges: []
    motion_planner:
      algorithm: "reference_line"
      horizon_distance_m: 15.0
      resample_interval_m: 0.2
      nearest_search_radius_m: 5.0
    speed_planner:
      algorithm: "curvature"
      default_speed_mps: 0.5
      max_speed_mps: 1.0
      max_lateral_accel_mps2: 0.5
```

## control_params.yaml

```yaml
low_speed_av_control:
  ros__parameters:
    output:
      mode: "scu_control_command"
    topics:
      localization_pose_topic: "/localization/pose"
      localization_pose_type: "pose_stamped"
      trajectory_topic: "/planning/trajectory"
      vehicle_state_topic: "/vehicle/state"
      safety_status_topic: "/safety/status"
      control_command_topic: "/control/command"
      scu_command_topic: "/yunle_chassis/control/scu_control_command"
      control_status_topic: "/control/status"
    controller:
      algorithm: "lqr"
      control_rate_hz: 50.0
      localization_timeout_s: 0.2
      trajectory_timeout_s: 0.5
    vehicle:
      model: "front_ackermann"
      wheel_base_m: 1.2
      track_width_m: 0.8
      max_speed_mps: 1.2
      max_accel_mps2: 0.5
      max_decel_mps2: 0.8
      max_front_steer_rad: 0.6
      max_rear_steer_rad: 0.6
      max_front_steer_rate_radps: 0.5
      max_rear_steer_rate_radps: 0.5
      rear_steer_ratio: 0.5
    pure_pursuit:
      lookahead_min_m: 0.8
      lookahead_max_m: 3.0
      lookahead_speed_gain: 1.2
    stanley:
      k: 0.8
      epsilon_mps: 0.1
    lqr:
      q_lateral_error: 3.0
      q_heading_error: 2.0
      r_steering: 1.0
      max_iterations: 80
      convergence_eps: 1.0e-6
      min_speed_mps: 0.2
      preview_time_s: 0.2
      use_curvature_feedforward: true
      max_steering_angle_rad: 0.52
    mpc_sampler:
      horizon_steps: 10
      dt_s: 0.1
      curvature_samples: [-0.2, -0.1, 0.0, 0.1, 0.2]
    command_smoother:
      max_speed_step_mps: 0.05
      max_accel_mps2: 0.5
      max_decel_mps2: 0.8
      max_steer_rate_radps: 0.35
      emergency_decel_mps2: 1.0
    scu:
      max_steering_angle_deg: 30.0
      max_target_speed_kmh: 5.0
      front_steer_sign: 1.0
      rear_steer_sign: 1.0
      stop_shift_level: 1
      torque_or_speed_mode: 1
      steering_angle_speed_valid: false
      brake_force_command_valid: false
      lights:
        left: 0
        right: 0
        position: 0
        low_beam: 0
```

## Launch Files

Generate:

```text
low_speed_av_planning/launch/planning.launch.py
low_speed_av_control/launch/control.launch.py
low_speed_av_bringup/launch/planning_control_demo.launch.py
```

Launch arguments:

```text
roadnet_package_path
planning_params_file
control_params_file
use_sim_time
```

## Runtime Workflow

```text
1. Start localization module externally.
2. Start planning node with roadnet package path.
3. Planning loads AD Package and publishes roadnet status.
4. Task manager or demo service requests route.
5. Planning publishes global route and trajectory.
6. Control subscribes trajectory and /localization/pose.
7. Control publishes /yunle_chassis/control/scu_control_command.
```
