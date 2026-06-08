# 接口审计 3

## Objective（目标）
审计 msg/srv 是否覆盖规划、控制、状态、前后轮转角、路线规划和算法切换需求，字段命名是否与工程文档和配置一致。

## Status（状态）
Pass with Runtime Not Verified。接口文件覆盖主要需求，字段设计满足静态合同；但 ROSIDL 生成与跨包编译未在 ROS2 环境验证。

## Evidence（证据）
- 生成接口列表：`src/low_speed_av_interfaces/CMakeLists.txt:8` 到 `src/low_speed_av_interfaces/CMakeLists.txt:19`。
- `ControlCommand` 前后轮转角：`src/low_speed_av_interfaces/msg/ControlCommand.msg:7` 到 `src/low_speed_av_interfaces/msg/ControlCommand.msg:8`。
- `ControlCommand` estop 字段：`src/low_speed_av_interfaces/msg/ControlCommand.msg:13`。
- `VehicleState` 前后轮转角反馈：`src/low_speed_av_interfaces/msg/VehicleState.msg:6` 到 `src/low_speed_av_interfaces/msg/VehicleState.msg:7`。
- `TrajectoryPoint` 使用 `kappa_1pm`、`s_m`、`v_mps`：`src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:11` 到 `src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:13`。
- `Trajectory` 包含 `emergency_stop` 和 `status_message`：`src/low_speed_av_interfaces/msg/Trajectory.msg:7` 到 `src/low_speed_av_interfaces/msg/Trajectory.msg:8`。
- `GlobalRoute` 包含 edge/node/length/time/status：`src/low_speed_av_interfaces/msg/GlobalRoute.msg:4` 到 `src/low_speed_av_interfaces/msg/GlobalRoute.msg:10`。
- `RoadnetStatus` 包含 validation/status 信息：`src/low_speed_av_interfaces/msg/RoadnetStatus.msg:4` 到 `src/low_speed_av_interfaces/msg/RoadnetStatus.msg:10`。
- `PlanRoute` response 返回 route，trajectory 由 topic 发布：`src/low_speed_av_interfaces/srv/PlanRoute.srv:8` 到 `src/low_speed_av_interfaces/srv/PlanRoute.srv:11`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-IF-001 | P3 | Pass | `ControlCommand` 已包含 `front_steering_angle_rad` 与 `rear_steering_angle_rad`。 |
| A3-IF-002 | P3 | Pass | planning/control 状态、路线、轨迹、车辆状态和算法切换服务齐全。 |
| A3-IF-003 | P2 | Not Verified | ROSIDL 代码生成和 C++ include 名称未通过真实 ROS2 构建验证。 |
| A3-IF-004 | P3 | Partial | `PlanRoute` 不直接返回 trajectory，依赖 topic 发布。设计可接受，但集成测试需要确认服务调用后 trajectory topic 可观测。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
接口静态设计足以支撑双 Ackermann 控制和规划服务。若 ROSIDL 生成失败或字段名在节点中引用不一致，ROS2 构建会失败，车辆运行前即可暴露。

## Recommended fix（推荐修复）
- 在 ROS2 环境运行接口生成和跨包编译。
- 增加一份接口字段契约测试或文档生成检查。
- 在集成计划中明确 `PlanRoute` 成功后检查 `/planning/trajectory`。

## Verification method（验证方法）
- 静态读取 `.msg`、`.srv` 和接口 CMake。
- 离线 smoke 验证 Python 镜像中的 command 字段为有限值。
- 未执行 ROSIDL 生成。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build --packages-select low_speed_av_interfaces`
- `ros2 interface show low_speed_av_interfaces/msg/ControlCommand`
- `ros2 interface show low_speed_av_interfaces/srv/PlanRoute`

