# 接口与话题审计

## 目标
审计自定义 msg/srv 完整性、控制指令转向字段、默认话题和话题可配置性。

## 状态
部分通过。

## 证据
- 接口 CMake 生成全部请求文件，见 `src/low_speed_av_interfaces/CMakeLists.txt:8` 至 `src/low_speed_av_interfaces/CMakeLists.txt:20`。
- `ControlCommand` 包含 `front_steering_angle_rad` 和 `rear_steering_angle_rad`，见 `src/low_speed_av_interfaces/msg/ControlCommand.msg:5` 和 `src/low_speed_av_interfaces/msg/ControlCommand.msg:6`。
- `TrajectoryPoint` 包含 `kappa_1pm`、`s_m`、`v_mps`、`gear`、`behavior`，见 `src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:9` 至 `src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:15`。
- `PlanRoute.srv` 返回 `GlobalRoute route`，见 `src/low_speed_av_interfaces/srv/PlanRoute.srv:9`。
- planning topics 可配置，见 `src/low_speed_av_planning/config/planning_params.yaml:9` 至 `src/low_speed_av_planning/config/planning_params.yaml:15`。
- control topics 可配置，见 `src/low_speed_av_control/config/control_params.yaml:3` 至 `src/low_speed_av_control/config/control_params.yaml:10`。
- planning node 声明了 `/localization/pose` 参数但未订阅，见 `src/low_speed_av_planning/src/planning_node.cpp:9`。

## 发现
### F-IT-001：请求的接口文件存在
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：项目具备基本消息/服务表面。
- 推荐修复：在 ROS2 环境中实际运行接口生成。
- 验证方法：有 ROS2 后执行 `colcon build --packages-select low_speed_av_interfaces`。

### F-IT-002：定位话题默认值正确且可配置
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：控制模块可通过 YAML 连接到既有定位输出。
- 推荐修复：如运行时需要，可增加 `PoseWithCovarianceStamped` 支持。
- 验证方法：参数测试使用非默认 localization topic。

### F-IT-003：服务已定义但节点未提供 service server
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：`ReloadRoadnet`、`PlanRoute`、`SetPlannerAlgorithm`、`SetControllerAlgorithm` 无法在运行时调用。
- 推荐修复：在 planning/control 节点中创建 service server 并实现 callback。
- 验证方法：ROS2 环境中执行 `ros2 service list` 和 service call 测试。

### F-IT-004：safety status 的消息类型合同不明确
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：配置中有 `/safety/status`，但代码没有确定使用何种消息类型，也没有 subscriber。
- 推荐修复：定义 safety status 输入类型，可用 `ModuleStatus` 或专用 safety message，并实现订阅。
- 验证方法：静态接口检查和 safety estop 集成测试。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `ros2 interface show low_speed_av_interfaces/msg/ControlCommand`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service list`
