# ROS2 Manual Validation Checklist

## Objective

提供人工验证记录表，配合 `ROS2_MANUAL_VALIDATION_PROCEDURE.md` 使用。

## Scope

Build、interfaces、roadnet、simulation、planning、current-pose start、control、SCU、安全和完整集成。

## Status

Not Verified。等待人工填写。

## Evidence

- `docs/audits_5/ROS2_MANUAL_VALIDATION_PROCEDURE.md` 给出了本检查表每个步骤对应的详细人工验证流程。
- `src/low_speed_av_interfaces/srv/PlanRoute.srv`、`src/low_speed_av_interfaces/srv/ReloadRoadnet.srv`、`src/low_speed_av_interfaces/msg/Trajectory.msg`、`src/low_speed_av_interfaces/msg/ControlCommand.msg` 是接口检查项来源。
- `src/low_speed_av_simulation/launch/simulation_visualization.launch.py`、`src/low_speed_av_bringup/launch/planning_control_demo.launch.py` 是 launch 检查项来源。

## Checklist

| Step | Command / Action | Expected Output | Human Confirmation | Pass/Fail | Notes | Blocking Issue ID |
|---|---|---|---|---|---|---|
| B1 | `source /opt/ros/$ROS_DISTRO/setup.bash` | ROS env loaded | `$ROS_DISTRO` valid |  |  |  |
| B2 | `rosdep install --from-paths src --ignore-src -r -y` | deps installed | no missing `chassis_interfaces` |  |  |  |
| B3 | `colcon build --symlink-install` | build success | all packages build |  |  |  |
| B4 | `colcon test` | tests run | no core failures |  |  |  |
| C1 | `ros2 interface show low_speed_av_interfaces/srv/PlanRoute` | expected fields | empty start semantics understood |  |  |  |
| C2 | `ros2 interface show chassis_interfaces/msg/ScuControlCommand` | expected SCU fields | no drive mode field |  |  |  |
| D1 | find roadnet package | folder exists | correct target package |  |  |  |
| D2 | inspect manifest/report | no blocking errors | status warning accepted |  |  |  |
| E1 | launch simulation visualization | RViz opens | roadnet visible |  |  |  |
| F1 | echo `/localization/pose` | PoseStamped updates | frame and quaternion valid |  |  |  |
| F2 | echo `/simulation/roadnet_markers` | non-empty MarkerArray | markers visible |  |  |  |
| F3 | call `/simulation/pause/start/reset` | success true | playback responds |  |  |  |
| G1 | launch planning/control | nodes alive | services/topics visible |  |  |  |
| G2 | echo `/planning/roadnet_status` | ready true | package id correct |  |  |  |
| H1 | call ReloadRoadnet | success true | roadnet status ready |  |  |  |
| I1 | explicit PlanRoute | success true | route/trajectory publish |  |  |  |
| J1 | empty-start PlanRoute | success true | start near current pose |  |  |  |
| K1 | stale pose failure | failure clear | no unsafe trajectory |  |  |  |
| K2 | invalid goal failure | failure clear | status explains |  |  |  |
| L1 | echo `/planning/trajectory` | non-empty | control input present |  |  |  |
| L2 | echo `/control/status` | active/stopping clear | status sensible |  |  |  |
| M1 | topic info SCU | correct type | exact topic/type |  |  |  |
| M2 | echo SCU normal | finite values | speed km/h non-negative |  |  |  |
| N1 | trigger estop | brake stop | brake true, speed 0 |  |  |  |
| N2 | localization timeout | brake stop | status timeout |  |  |  |
| O1 | pure_pursuit | command finite | no unsafe output |  |  |  |
| O2 | stanley | command finite | no unsafe output |  |  |  |
| O3 | lqr | command finite | no Stanley fallback |  |  |  |
| O4 | mpc_sampler | command finite | deterministic output |  |  |  |
| P1 | full integration | dataflow works | no unexpected motion |  |  |  |

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-CHK-001 | P1 | Not Verified | Checklist has not been executed. | Release readiness unknown. | Execute and attach logs. | Completed table with pass/fail. |

## ROS2 Commands Run Or Skipped

SKIPPED_ROS2_UNAVAILABLE in current environment.

## Remaining Uncertainty

All rows are open until filled by an operator in ROS2 environment.
