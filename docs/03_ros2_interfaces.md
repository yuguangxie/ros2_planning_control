# 03 ROS2 Interfaces

This document defines recommended custom messages. Codex may adjust minor details if required by ROS2 IDL syntax, but must preserve semantic fields.

## Messages

### TrajectoryPoint.msg

```text
std_msgs/Header header
uint32 index
string waypoint_id
string edge_id
string path_id
float64 x_m
float64 y_m
float64 yaw_rad
float64 kappa_1pm
float64 s_m
float64 v_mps
float64 a_mps2
float64 relative_time_s
int8 gear
string behavior
```

Gear constants can be documented in comments or a separate enum-like message:

```text
0 UNKNOWN
1 DRIVE
2 REVERSE
3 PARK
```

### Trajectory.msg

```text
std_msgs/Header header
string trajectory_id
string source_package_id
string planner_algorithm
TrajectoryPoint[] points
bool emergency_stop
string status
```

### GlobalRoute.msg

```text
std_msgs/Header header
string route_id
string source_package_id
string planner_algorithm
string[] node_ids
string[] edge_ids
float64 length_m
float64 estimated_time_s
string status
```

### ControlCommand.msg

```text
std_msgs/Header header
float64 speed_mps
float64 acceleration_mps2
float64 steering_angle_rad
float64 front_steering_angle_rad
float64 rear_steering_angle_rad
float64 brake
int8 gear
bool enable
bool emergency_stop
string controller_algorithm
string vehicle_model
string reason
```

`steering_angle_rad` is kept as a compatibility alias for front steering. For dual Ackermann, use both front and rear steering fields.

### VehicleState.msg

```text
std_msgs/Header header
float64 speed_mps
float64 acceleration_mps2
float64 steering_angle_rad
float64 front_steering_angle_rad
float64 rear_steering_angle_rad
int8 gear
bool autonomous_enabled
bool brake_pressed
string fault_code
```

### ModuleStatus.msg

```text
std_msgs/Header header
string module_name
string state
uint8 level
string message
```

### RoadnetStatus.msg

```text
std_msgs/Header header
string package_id
string schema_version
string validation_status
uint32 nodes
uint32 edges
uint32 waypoints
bool ready
string message
```

## Services

### ReloadRoadnet.srv

```text
string package_path
---
bool success
string package_id
string message
```

### PlanRoute.srv

```text
string start_node_id
string goal_node_id
string start_task_point_id
string goal_task_point_id
string goal_parking_point_id
---
bool success
string message
GlobalRoute route
```

### SetPlannerAlgorithm.srv

```text
string global_planner_algorithm
string motion_planner_algorithm
string speed_planner_algorithm
---
bool success
string message
```

### SetControllerAlgorithm.srv

```text
string controller_algorithm
string vehicle_model
---
bool success
string message
```

## Topics

Default topics must be configurable:

```yaml
topics:
  localization_pose_topic: "/localization/pose"
  localization_pose_type: "pose_stamped"
  trajectory_topic: "/planning/trajectory"
  global_route_topic: "/planning/global_route"
  planning_status_topic: "/planning/status"
  roadnet_status_topic: "/planning/roadnet_status"
  vehicle_state_topic: "/vehicle/state"
  safety_status_topic: "/safety/status"
  control_command_topic: "/control/command"
  control_status_topic: "/control/status"
```

## QoS

Recommended:

- localization: sensor data or best effort with timeout guard.
- trajectory/control/status: reliable.
- status: transient local optional.
