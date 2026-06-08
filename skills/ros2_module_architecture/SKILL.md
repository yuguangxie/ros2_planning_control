# Skill: ROS2 Module Architecture Without ROS2 Runtime

Use this skill when creating ROS2 packages in a Codex environment that may not have ROS2 installed.

## Package Layout

Generate:

```text
src/low_speed_av_interfaces
src/low_speed_av_planning
src/low_speed_av_control
src/low_speed_av_bringup
```

Planning and control must be separate packages. Interfaces must not depend on planning/control implementation.

## Source Language

Use C++17 for ROS2 nodes and core runtime code. Keep algorithm core classes as plain C++ as much as possible so they can be reviewed without ROS2 runtime.

## Interface Package

`low_speed_av_interfaces` contains only `msg`, `srv`, `action`, `CMakeLists.txt`, `package.xml`.

Recommended messages:

```text
TrajectoryPoint.msg
Trajectory.msg
GlobalRoute.msg
ControlCommand.msg
VehicleState.msg
ModuleStatus.msg
RoadnetStatus.msg
```

Recommended services:

```text
PlanRoute.srv
ReloadRoadnet.srv
SetPlannerAlgorithm.srv
SetControllerAlgorithm.srv
```

## Planning Package

Main components:

```text
include/low_speed_av_planning/roadnet_loader.hpp
include/low_speed_av_planning/global_planner.hpp
include/low_speed_av_planning/motion_planner.hpp
include/low_speed_av_planning/speed_planner.hpp
include/low_speed_av_planning/planning_node.hpp
src/*.cpp
config/planning_params.yaml
launch/planning.launch.py
```

## Control Package

Main components:

```text
include/low_speed_av_control/vehicle_model.hpp
include/low_speed_av_control/controller_base.hpp
include/low_speed_av_control/pure_pursuit_controller.hpp
include/low_speed_av_control/stanley_controller.hpp
include/low_speed_av_control/lqr_controller.hpp
include/low_speed_av_control/mpc_sampler_controller.hpp
include/low_speed_av_control/command_smoother.hpp
include/low_speed_av_control/control_node.hpp
src/*.cpp
config/control_params.yaml
launch/control.launch.py
```

## No ROS2 Runtime Rule

If `colcon` or ROS2 headers are missing, do not remove ROS2 files. Instead:

- write correct source files,
- add clear package.xml/CMakeLists,
- create offline Python validators,
- create phase report marking ROS2 build as skipped.

## Launch and Config

All topic names and algorithms must be configurable by YAML. Default localization topic is `/localization/pose`.
