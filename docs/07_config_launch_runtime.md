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
      mode: "both"
    topics:
      localization_pose_topic: "/localization/pose"
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
      allowed_trajectory_statuses: ["ok"]
      trajectory_s_tolerance_m: 1.0e-4
      progress_backward_window_points: 3
      progress_forward_window_points: 200
      progress_max_heading_error_rad: 1.57
    control:
      status_publish_rate_hz: 5.0
      publish_deadline_warning_s: 0.1
      cadence_window_size: 128
    hardware_watchdog:
      timeout_s: 0.5
      contract_status: "DECLARED_NOT_HIL_VERIFIED"
    vehicle_state:
      required: false
      timeout_s: 0.5
    safety:
      estop_latched: true
      clear_speed_threshold_mps: 0.05
    vehicle:
      model: "front_ackermann"
      wheel_base_m: 1.2
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
      max_accel_mps2: 0.5
      max_decel_mps2: 0.8
      max_jerk_mps3: 2.0
      min_dt_s: 0.001
      max_dt_s: 0.1
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

`hardware_watchdog.timeout_s` 是外部硬件合同的诊断上界，不是软件 watchdog 周期；生产校验不允许它大于项目声明的 `0.5 s`。`contract_status` 保持 `DECLARED_NOT_HIL_VERIFIED`，直至供应商资料和台架证据归档后才可变更。

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
7. Control defaults to publishing `/control/command`, `/control/status`, and `/yunle_chassis/control/scu_control_command`.
```

Control 的 localization/trajectory/VehicleState watchdog 使用本地 steady-clock receive time；不会把零 header stamp 或暂停的仿真时间直接判为消息超时。锁存急停只能通过 `/low_speed_av_control/clear_estop`（`std_srvs/srv/Trigger`）显式清除，并且清除后先进入 `READY`。

运行边界：simulation 只验证软件消息语义；bench 必须断开驱动轮或架车并具备物理急停；vehicle 必须额外验证底盘硬件 watchdog。当前 Yunle Chassis Driver 尚无本阶段要求的独立周期命令 watchdog，不能把 Control 持续发布 stop 当成等价替代。
# Phase 18 closed-loop SIL runtime

闭环软件仿真入口为：

```bash
ros2 launch low_speed_av_bringup planning_control_closed_loop_sim.launch.py \
  rviz:=false controller_algorithm:=lqr vehicle_model:=front_ackermann
```

数据链为 `Planning trajectory -> Control -> /control/command -> Simulation plant -> /localization/pose + /vehicle/state`。闭环专用 `control_sim_params.yaml` 使用 internal-only output；生产 Control 配置仍为 both。`path_replay` 仍保留用于 Planning 快速检查，但不能作为 controller 闭环证据。

闭环 launch 为了保持稳定 trajectory identity，关闭 Planning 的重复 local crop；Control 的 production progress tracker 仍根据当前 pose 执行有界窗口裁剪。它不启动 Chassis、keyboard、UDP 或 CAN。
