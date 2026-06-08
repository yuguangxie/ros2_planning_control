# Phase 01 — Generate ROS2 Interface Package

请生成或更新：

```text
src/low_speed_av_interfaces/
```

必须包含：

```text
msg/TrajectoryPoint.msg
msg/Trajectory.msg
msg/GlobalRoute.msg
msg/ControlCommand.msg
msg/VehicleState.msg
msg/ModuleStatus.msg
msg/RoadnetStatus.msg
srv/ReloadRoadnet.srv
srv/PlanRoute.srv
srv/SetPlannerAlgorithm.srv
srv/SetControllerAlgorithm.srv
CMakeLists.txt
package.xml
```

接口语义要求：

1. `TrajectoryPoint` 包含 `x_m/y_m/yaw_rad/kappa_1pm/s_m/v_mps/a_mps2/relative_time_s/gear/behavior/edge_id/waypoint_id/path_id`。
2. `ControlCommand` 包含 `speed_mps/acceleration_mps2/steering_angle_rad/front_steering_angle_rad/rear_steering_angle_rad/brake/gear/enable/emergency_stop/controller_algorithm/vehicle_model/reason`。
3. `GlobalRoute` 包含 `node_ids/edge_ids/length_m/estimated_time_s/status`。
4. `RoadnetStatus` 包含 package/schema/validation/ready/counts。

不要实现规划或控制算法。本阶段只做 interfaces。

创建：

```text
reports/phase_01_report.md
```

报告中说明没有运行 colcon，因为当前环境没有 ROS2。
