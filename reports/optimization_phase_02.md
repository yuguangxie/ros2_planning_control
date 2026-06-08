# Optimization Phase 02 Report

- Goal: Implement control node normal tracking pipeline.
- Files changed: `src/low_speed_av_control/include/low_speed_av_control/control_node.hpp`, `src/low_speed_av_control/src/control_node.cpp`, `src/low_speed_av_control/src/command_limiter.cpp`.
- Design decisions: Control timer now prioritizes safety/timeout/empty-trajectory stops before normal tracking; normal tracking uses `ControllerFactory`, `VehicleModelFactory`, `CommandLimiter`, and `CommandSmoother`.
- Audit findings addressed: F-002/P0, F-CT-002, F-CT-004, and F-CT-005.
- AD Package compatibility: Control remains decoupled from AD Package and consumes only planning trajectory messages.
- Topic/config compatibility: Subscribes configurable localization, trajectory, vehicle state, and safety status topics; publishes configurable control command/status topics; `/localization/pose` remains default.
- Offline checks: `offline_algorithm_smoke.py` now covers finite Pure Pursuit/Stanley commands and finite front/dual Ackermann conversion.
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic pub /planning/trajectory ...`; `ros2 topic echo /control/command`; `colcon test`.
- Known limits: LQR and MPC sampler remain lightweight skeletons; true actuator behavior must be verified on ROS2/hardware simulator.
- Next steps: Add node-level ROS2 integration test when ROS2 is available.
