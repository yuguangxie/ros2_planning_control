# 接口、话题与配置第二轮审计

## Objective
审计自定义 msg/srv 是否完整，规划/控制节点是否提供对应服务，话题默认值是否符合要求，尤其是 `/localization/pose` 默认且可配置，以及前/后轮转角接口是否存在。

## Status: Partial
接口文件和配置表面通过；service server 已在源码层面创建；ROS2 接口生成和 `ros2 service list` 未验证。

## Evidence
- 接口生成清单：`src/low_speed_av_interfaces/CMakeLists.txt:8` 至 `src/low_speed_av_interfaces/CMakeLists.txt:19`。
- `PlanRoute.srv` 返回 `GlobalRoute route`，见 `src/low_speed_av_interfaces/srv/PlanRoute.srv:8` 至 `src/low_speed_av_interfaces/srv/PlanRoute.srv:11`。
- `SetPlannerAlgorithm.srv` 定义三类规划算法切换字段。
- `SetControllerAlgorithm.srv` 定义 controller 和 vehicle model 切换字段。
- `ModuleStatus.msg` 可作为 safety status 输入，字段见 `src/low_speed_av_interfaces/msg/ModuleStatus.msg:1` 至 `src/low_speed_av_interfaces/msg/ModuleStatus.msg:5`。
- `ControlCommand` 前后轮转角字段：`src/low_speed_av_interfaces/msg/ControlCommand.msg:7` 至 `src/low_speed_av_interfaces/msg/ControlCommand.msg:8`。
- planning topic 默认值：`src/low_speed_av_planning/src/planning_node.cpp:23` 至 `src/low_speed_av_planning/src/planning_node.cpp:27`。
- control topic 默认值：`src/low_speed_av_control/src/control_node.cpp:14` 至 `src/low_speed_av_control/src/control_node.cpp:19`。
- control config 中 `/localization/pose` 默认：`src/low_speed_av_control/config/control_params.yaml:12`。

## Findings
### A2-ITC-001：接口清单完整
- Severity: P3
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: planning/control 有统一消息和服务合同。
- Recommended fix: 在 ROS2 环境验证接口生成。
- Verification method: `colcon build --packages-select low_speed_av_interfaces` 和 `ros2 interface show`。

### A2-ITC-002：规划/控制 service server 已创建
- Severity: P1
- Status: Pass by static audit / Not Verified by ROS2 runtime
- Impact on planning/control/vehicle operation: 外部任务管理器理论上可以 reload roadnet、plan route、切换算法。
- Recommended fix: 运行 `ros2 service list` 和 service call。
- Verification method: ROS2 环境检查服务名称和响应。

### A2-ITC-003：定位话题默认且可配置
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 默认能适配 `/localization/pose`，也能通过 YAML/launch 改接其他定位输出。
- Recommended fix: 后续可增加 `PoseWithCovarianceStamped` 支持。
- Verification method: 修改 `topics.localization_pose_topic` 后确认订阅目标改变。

### A2-ITC-004：安全输入类型已确定为 ModuleStatus
- Severity: P2
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 安全模块可用统一状态消息触发控制急停。
- Recommended fix: 文档明确 `level>=2` 和 `state=estop|emergency_stop|failure` 的语义。
- Verification method: 发布不同 ModuleStatus 组合并检查控制输出。

### A2-ITC-005：接口生成未验证
- Severity: P1
- Status: Not Verified
- Impact on planning/control/vehicle operation: 若 CMake/package.xml 或依赖声明有误，ROS2 build 时接口可能无法生成。
- Recommended fix: 在 ROS2 环境执行接口包 build/test。
- Verification method: `colcon build --packages-select low_speed_av_interfaces`。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 interface show low_speed_av_interfaces/msg/ControlCommand`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 interface show low_speed_av_interfaces/srv/PlanRoute`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service list`
- SKIPPED_ROS2_UNAVAILABLE: `colcon build --packages-select low_speed_av_interfaces`

