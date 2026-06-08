# 06 Four-Wheel Ackermann Vehicle Model

## Supported Models

```yaml
vehicle:
  model: "front_ackermann"   # front_ackermann | dual_ackermann
```

## Required Parameters

```yaml
vehicle:
  wheel_base_m: 1.2
  track_width_m: 0.8
  vehicle_length_m: 1.8
  vehicle_width_m: 0.9
  rear_axle_to_center_m: 0.0
  max_speed_mps: 1.2
  max_accel_mps2: 0.5
  max_decel_mps2: 0.8
  max_front_steer_rad: 0.6
  max_rear_steer_rad: 0.6
  max_front_steer_rate_radps: 0.5
  max_rear_steer_rate_radps: 0.5
  rear_steer_ratio: 0.5
```

## Front Ackermann

```text
kappa = tan(delta_front) / L
```

```text
delta_front = atan(kappa * L)
delta_rear = 0
```

## Dual Ackermann Counter-Phase

```text
kappa = (tan(delta_front) - tan(delta_rear)) / L
```

With ratio:

```text
tan(delta_rear) = -r * tan(delta_front)
tan(delta_front) = kappa * L / (1 + r)
```

Then:

```text
delta_front = atan(kappa * L / (1 + r))
delta_rear = atan(-r * tan(delta_front))
```

## Command Message

The command must carry both front and rear steering angles:

```text
front_steering_angle_rad
rear_steering_angle_rad
```

`steering_angle_rad` can mirror `front_steering_angle_rad` for older chassis adapters.

## Validation

Vehicle model should expose:

```text
curvatureToSteering(kappa)
steeringToCurvature(front, rear)
clampCommand(command)
```

The planner can also use vehicle parameters to reject route edges with curvature beyond capability.
