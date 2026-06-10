# ROS2 Manual Validation Procedure

## Objective

提供完整人工 ROS2 测试流程，用于在真实 Ubuntu/ROS2 环境验证 interfaces、roadnet、planning、current-pose start、simulation visualization、control、SCU output 和 safety。

## Scope

所有功能模块和端到端链路。

## Status

Not Verified in current environment. This is a human-executable validation plan.

## Evidence

- `src/low_speed_av_interfaces/msg/*.msg` 和 `src/low_speed_av_interfaces/srv/*.srv` 是接口验证命令的依据。
- `src/low_speed_av_simulation/launch/simulation_visualization.launch.py`、`src/low_speed_av_bringup/launch/planning_control_demo.launch.py` 是 launch 验证命令的依据。
- `src/low_speed_av_planning/src/planning_node.cpp`、`src/low_speed_av_control/src/control_node.cpp` 和 `src/low_speed_av_control/src/scu_command_mapper.cpp` 是规划、控制、SCU 输出验证项的依据。
- `roadnet_ad_package_20260610T012525Z/` 是当前人工验证使用的目标路网包。

## A. Safety Preconditions

- 使用 Ubuntu 22.04 或目标 ROS2 发行版环境。
- workspace 中包含所有包：interfaces、planning、control、simulation、bringup、`chassis_interfaces`。
- 车辆必须禁用、轮离地或处于 bench-only 环境。
- 不得在无 E-stop、无操作员、无封闭安全场地时对真实车辆发送运动命令。
- 模拟定位不得接入 live vehicle，除非明确进行台架验证且 chassis output 已受控。
- 当前 `roadnet_ad_package_20260610T012525Z` 有 validation warning，初次测试应低速。

## B. Build Validation

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

Human confirmations:

- No missing `chassis_interfaces`.
- No catkin/roscpp errors.
- Interface generation succeeds.
- planning package builds.
- control package builds.
- simulation package builds.
- bringup package builds.

## C. Interface Validation

```bash
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/srv/ReloadRoadnet
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
ros2 interface show chassis_interfaces/msg/ScuControlCommand
```

Confirm:

- `PlanRoute` request has `start_node_id`, `goal_node_id`, `start_task_point_id`, `goal_task_point_id`, `goal_parking_point_id`.
- Empty start behavior is implemented in planning code/config, not a new srv field.
- `ScuControlCommand` has expected SCU fields.
- Project code does not expect `scu_drive_mode_request`.

## D. Roadnet Validation

```bash
find . -type d -name "roadnet_ad_package_20260610T012525Z"
```

Inspect:

```bash
ls roadnet_ad_package_20260610T012525Z
cat roadnet_ad_package_20260610T012525Z/project_manifest.json
cat roadnet_ad_package_20260610T012525Z/validation/validation_report.json
```

Confirm:

- Folder exists.
- `project_manifest.json` exists.
- `checksums.sha256` exists.
- `roadnet/topology.json` exists.
- `trajectory/waypoints.yaml` exists.
- `trajectory/waypoint_index.json` exists.
- validation has no blocking errors.
- frame is `map`.

## E. Simulation Visualization Launch

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

Confirm:

- RViz opens.
- Fixed frame is `map`.
- Roadnet markers visible.
- Vehicle pose marker visible.
- Semantic markers visible if data exists.
- `/localization/pose` is publishing.

## F. Simulation Topic Validation

```bash
ros2 topic list
ros2 topic echo /localization/pose
ros2 topic hz /localization/pose
ros2 topic echo /simulation/roadnet_markers
ros2 topic echo /simulation/vehicle_markers
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
```

Confirm:

- Pose timestamp updates.
- Pose frame id is `map` or configured frame.
- Quaternion norm is approximately 1.
- Publish rate matches config.
- Markers are non-empty.
- Start/pause/reset services respond success.

## G. Planning Launch And Roadnet Readiness

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z
```

In another terminal:

```bash
ros2 node list
ros2 service list
ros2 topic list
ros2 topic echo /planning/roadnet_status
ros2 param get /low_speed_av_planning roadnet.package_path
ros2 param get /low_speed_av_planning planning.use_current_pose_as_start
```

Confirm:

- Planning node is running.
- Roadnet status ready.
- Plan route service exists.
- Reload roadnet service exists.
- Current-pose start parameter is true unless intentionally disabled.

## H. Reload Roadnet

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/roadnet_ad_package_20260610T012525Z'}"
```

Confirm:

- response success true.
- package id shown.
- `/planning/roadnet_status.ready` true.

## I. Explicit-Start Planning Regression Test

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

Then:

```bash
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /planning/status
```

Confirm:

- service returns success true.
- route edge ids include expected route.
- trajectory has non-empty points.
- RViz route and trajectory overlays appear.

## J. Current-Pose-Start Planning Test

First ensure `/localization/pose` is publishing near roadnet.

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

Confirm:

- planner uses current pose as start.
- response success true.
- status message or logs identify matched waypoint/edge/start node.
- route begins near simulated vehicle.
- trajectory begins near current pose vicinity.

## K. Current-Pose Failure Tests

Test stale pose:

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
sleep 2
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

Confirm:

- planner fails clearly with stale pose message.
- no unsafe high-speed trajectory is published.
- `/planning/status` explains the reason.

Test invalid goal:

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'BAD_NODE', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

Confirm failure is clear.

## L. Control Input Validation

```bash
ros2 topic echo /planning/trajectory
ros2 topic echo /control/status
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic hz /yunle_chassis/control/scu_control_command
```

Confirm:

- control receives trajectory.
- control receives localization.
- control status is healthy when inputs are fresh.
- SCU command publishes finite values, or safe stop under invalid conditions.

## M. SCU Command Validation

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/control/scu_control_command
```

Confirm:

- topic exactly `/yunle_chassis/control/scu_control_command`.
- type exactly `chassis_interfaces/msg/ScuControlCommand`.
- speed is km/h.
- speed is non-negative.
- reverse uses shift 3, not negative speed.
- neutral uses shift 2 if commanded.
- drive uses shift 1.
- steering is degrees.
- invalid shift is never published.
- stop command has brake true, speed 0, front/rear steering 0.

## N. Safety Validation

Trigger safety estop:

```bash
ros2 topic pub --once /safety/status low_speed_av_interfaces/msg/ModuleStatus \
  "{module_name: 'manual_test', state: 'estop', level: 2, message: 'manual estop'}"
```

Confirm:

- `/control/status` reports safety.
- SCU command has `scu_brake_enable=true`.
- `scu_target_speed=0`.
- front/rear steering are 0.
- shift is valid.

Trigger localization timeout by pausing simulation and waiting longer than timeout. Confirm same safe stop behavior.

## O. Controller Validation

```bash
ros2 param set /low_speed_av_control controller.algorithm pure_pursuit
ros2 param set /low_speed_av_control controller.algorithm stanley
ros2 param set /low_speed_av_control controller.algorithm lqr
ros2 param set /low_speed_av_control controller.algorithm mpc_sampler
```

Confirm:

- supported algorithms are accepted or clearly rejected if runtime parameter update is not supported.
- LQR output is finite.
- LQR does not log Stanley fallback.
- curved trajectory creates sensible feedforward steering.
- low-speed behavior remains finite.

## P. Full Integration Validation

Run together:

- simulation pose publisher
- roadnet visualization
- planning node
- control node
- optional chassis driver in bench mode

Confirm:

- roadnet visible.
- vehicle pose visible.
- planning from current pose works.
- route/trajectory visible.
- control command updates.
- estop overrides control.
- no unexpected motion command is produced.

## Q. Pass/Fail Checklist

Use `ROS2_MANUAL_VALIDATION_CHECKLIST.md` for tabular recording.

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-MAN-001 | P1 | Not Verified | Full manual procedure not run yet. | Real readiness unknown. | Execute sections A-Q in order. | Fill checklist. |

## ROS2 Commands Run Or Skipped

SKIPPED_ROS2_UNAVAILABLE in current environment. All commands above are intended for a real ROS2 shell.

## Remaining Uncertainty

All runtime outcomes remain unknown until this procedure is executed.
