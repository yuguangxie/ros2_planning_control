# 05 Control Module Design

## Node

```text
low_speed_av_control_node
```

## Main Classes

```text
VehicleModelBase
FrontAckermannModel
DualAckermannModel
ControllerBase
PurePursuitController
StanleyController
LqrController
MpcSamplerController
CommandLimiter
CommandSmoother
TrackingErrorEstimator
ControlNode
```

## Inputs

```text
/planning/trajectory
/localization/pose       # default, configurable
/vehicle/state
/safety/status
```

## Outputs

```text
/yunle_chassis/control/scu_control_command
/control/command   # optional debug/internal output
/control/status
```

## Algorithm Selection

```yaml
controller:
  algorithm: "lqr"            # pure_pursuit | stanley | lqr | mpc_sampler
vehicle:
  model: "front_ackermann"    # front_ackermann | dual_ackermann
```

Runtime service:

```text
SetControllerAlgorithm.srv
```

## Pure Pursuit

1. Find nearest trajectory point.
2. Compute adaptive lookahead:

```text
lookahead = clamp(min + speed * gain, min, max)
```

3. Find target point by route `s_m`.
4. Transform target to vehicle rear axle frame.
5. Compute curvature and convert to steering through vehicle model.
6. Speed command from target `v_mps` with smoother.

## Stanley

1. Find nearest point.
2. Compute heading error.
3. Compute signed lateral error.
4. Steering command:

```text
steer = heading_error + atan2(k * lateral_error, abs(speed) + epsilon)
```

5. Clamp low-speed jitter.

## LQR

Production-oriented implementation uses a two-state discrete kinematic-bicycle LQR:

```text
x = [e_y, e_psi]^T
A = [[1, v*dt],
     [0, 1]]
B = [[0],
     [v*dt/wheel_base]]
P_next = A^T P A - A^T P B (R + B^T P B)^-1 B^T P A + Q
delta_cmd = atan(wheel_base * kappa_ref) - K * [e_y, e_psi]^T
desired_curvature_1pm = tan(delta_cmd) / wheel_base
```

The vehicle model converts `desired_curvature_1pm` to front/rear physical steering angles.

## Yunle SCU Output

`ScuCommandMapper` converts the internal SI command to `chassis_interfaces/msg/ScuControlCommand`.

```text
topic = /yunle_chassis/control/scu_control_command
speed = abs(speed_mps) * 3.6 km/h
steering = rad to deg
shift = 1 D, 2 N, 3 R
```

Unknown gear or safety stop maps to a brake stop command and never publishes an invalid shift value.

## MPC Sampler

Use a lightweight deterministic sampler:

```text
for each curvature_candidate:
  roll out kinematic model for horizon
  compute cost to trajectory
select min cost
```

This avoids heavy solver dependencies and is appropriate for Codex-generated MVP.

## Safety Stop

Trigger controlled stop if:

```text
localization timeout
trajectory timeout
empty trajectory
safety estop
invalid NaN command
algorithm exception
```

Default stop command:

```text
speed_mps = 0
acceleration_mps2 = -abs(emergency_decel)
brake = true
enable = false or config-controlled
reason = timeout/estop/error
```
