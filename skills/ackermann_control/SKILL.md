# Skill: Ackermann Control Algorithms

Use this skill for vehicle model and controller implementation.

## Inputs

Control node consumes:

```text
/planning/trajectory
/localization/pose    # default, configurable
/vehicle/state
/safety/status
```

Output:

```text
/control/command
/control/status
```

## Vehicle Models

Support:

```text
front_ackermann
dual_ackermann
```

Front Ackermann:

```text
kappa = tan(delta_front) / L
delta_front = atan(L * kappa)
delta_rear = 0
```

Dual Ackermann counter-phase:

```text
kappa = (tan(delta_front) - tan(delta_rear)) / L
tan(delta_rear) = -rear_steer_ratio * tan(delta_front)
tan(delta_front) = kappa * L / (1 + rear_steer_ratio)
delta_front = atan(tan_delta_front)
delta_rear = atan(-rear_steer_ratio * tan_delta_front)
```

Clamp both steering angles and steering rate.

## Algorithms

### Pure Pursuit

Inputs: pose, trajectory, wheel_base, lookahead.

Steps:

1. Find nearest trajectory point.
2. Find lookahead point along `s_m`.
3. Transform target to vehicle frame.
4. `kappa_cmd = 2 * y_vehicle / lookahead^2`.
5. Convert curvature to steering through selected vehicle model.
6. Use target point `v_mps` for speed command.

### Stanley

Inputs: pose, trajectory, current speed, `stanley_k`.

```text
steer_curvature_or_angle = heading_error + atan2(k * cross_track_error, abs(v) + epsilon)
```

Convert desired curvature or front steering through vehicle model. Limit low-speed oscillation.

### LQR

Implement a practical low-speed LQR tracker skeleton:

- State error: lateral error, lateral error rate, heading error, heading error rate.
- Use configurable gains initially; optional Riccati solver can be added later.
- Feedforward curvature from trajectory `kappa_1pm`.

### MPC Sampler

Implement deterministic sampler, not a heavy optimizer:

- Sample curvature/steering candidates around reference curvature.
- Roll out simple bicycle/dual Ackermann kinematics.
- Cost = lateral error + heading error + speed error + steering effort + curvature smoothness.
- Select lowest cost and publish command.

## Command Smoother

Apply:

```text
max_accel
max_decel
max_speed_step
max_steer_rate
max_front_steer_rad
max_rear_steer_rad
emergency_decel
```

## Safety

If localization timeout, trajectory timeout, empty trajectory, invalid command, or safety estop occurs, publish controlled stop:

```text
speed_mps = 0
acceleration_mps2 <= 0
brake = true
enable = false or keep_enable_for_controlled_stop according to config
```
