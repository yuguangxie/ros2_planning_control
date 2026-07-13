# Phase 18 Simulation Test Matrix

| Layer | Production target | Coverage |
|---|---|---|
| C++ plant | `low_speed_av_simulation_core` / `test_simulation_core` | straight/curvature, front/dual Ackermann, accel/decel/jerk, independent steer rates, brake/emergency/disable, invalid dt/options/NaN/gear/reverse, timeout, reset, determinism, goal/yaw tolerance |
| C++ monitor | same core/target | explicit cadence max/p95, lateral/heading max/RMS/p95, goal errors, timeout/non-finite counters, reset |
| Simulation ROS | production simulation node | ControlCommand-driven pose, Plant VehicleState, timeout stop, reset/history, diagnostics, internal-only/no Chassis, bounded exit |
| Full SIL | Planning + Control + production plant | sample ready, N0001→N0003, ACTIVE, four controllers × front/dual Ackermann, finite trajectory/commands/state, goal stop, metrics, no SCU message, no Chassis, bounded exit |
| Existing safety integration | production Planning/Control | Planning failure, localization/trajectory/VehicleState timeout, disabled/brake/fault, estop latch/clear, controller READY/reset, late subscriber |

Initial SIL gates remain: Control/localization max interval `<0.15 s`, lateral RMS `<0.4 m`, lateral max `<0.8 m`, goal position `<0.3 m`, goal yaw `<0.35 rad`, stopped speed `<0.05 m/s`, non-finite count `0`. These are SIL gates only.
