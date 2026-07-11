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
/control/command   # default contract output
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

## Safety State Machine

安全状态机独立于控制器和车辆模型，状态为：

```text
WAIT_INPUTS -> READY -> ACTIVE
                    -> CONTROLLED_STOP
                    -> ESTOP_LATCHED
```

优先级从高到低为：安全急停/显式 emergency trajectory；车辆故障、自治关闭、人工制动；定位非法或超时；轨迹非法、Planning failure 或超时；正常跟踪。只有 `ACTIVE` 可以调用控制器。`Trajectory.status` 默认仅允许 `ok`，空点、NaN/Inf、非法 gear、明显非单调 `s_m` 全部 fail closed。

所有停车输出统一为 `speed_mps=0`、前后轮转角为 0、`brake=1`、`enable=false`、非空稳定 reason；SCU 映射为 target speed 0 和 brake enable true。安全停车绕过 normal smoother，防止旧运动命令延迟停车。`CONTROLLED_STOP` 表示输入失效或门控停车；`ESTOP_LATCHED` 表示高优先级锁存急停。由于当前 SCU 只有布尔制动字段，两者在底盘报文层都映射为 brake enable，制动力曲线仍由底盘侧实现。

急停只能通过 `std_srvs/srv/Trigger` 服务 `/low_speed_av_control/clear_estop` 显式清除。清除成功先回到 `READY`，不会在 service callback 中发布运动命令。

Control watchdog 基于 steady-clock 接收时间，负责检测定位、轨迹和 VehicleState 的新鲜度。底盘驱动独立命令 watchdog 是另一层职责；本轮按范围约束未修改 Yunle Chassis，因此进程级/硬件级命令超时仍是已知风险。即使以后补齐 driver watchdog，驱动进程硬崩溃、断电或网关故障仍必须依赖底盘硬件 watchdog。
