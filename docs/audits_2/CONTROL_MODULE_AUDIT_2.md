# 控制模块第二轮审计

## Objective
审计优化后的 control package 是否实现正常轨迹跟踪、控制器工厂、车辆模型工厂、限幅、平滑、NaN/Inf guard、timeout 和 service 切换。

## Status: Partial
控制节点源码层面已从“只停车分支”升级为正常跟踪 pipeline。ROS2 runtime 未验证，LQR/MPC 仍为骨架成熟度。

## Evidence
- topic 和参数声明：`src/low_speed_av_control/src/control_node.cpp:14` 至 `src/low_speed_av_control/src/control_node.cpp:39`。
- pose/trajectory/vehicle/safety 订阅：`src/low_speed_av_control/src/control_node.cpp:45` 至 `src/low_speed_av_control/src/control_node.cpp:56`。
- `SetControllerAlgorithm` 服务：`src/low_speed_av_control/src/control_node.cpp:61` 至 `src/low_speed_av_control/src/control_node.cpp:66`。
- controller/vehicle model 实例化：`src/low_speed_av_control/src/control_node.cpp:80` 至 `src/low_speed_av_control/src/control_node.cpp:83`。
- 正常跟踪计算入口：`src/low_speed_av_control/src/control_node.cpp:160`。
- 转角到曲率再到车辆模型：`src/low_speed_av_control/src/control_node.cpp:169` 至 `src/low_speed_av_control/src/control_node.cpp:173`。
- limiter/smoother：`src/low_speed_av_control/src/control_node.cpp:184` 至 `src/low_speed_av_control/src/control_node.cpp:189`。
- timer 正常分支：`src/low_speed_av_control/src/control_node.cpp:216`。
- Pure Pursuit lookahead/steering：`src/low_speed_av_control/src/pure_pursuit_controller.cpp:41` 至 `src/low_speed_av_control/src/pure_pursuit_controller.cpp:57`。
- Stanley heading/lateral correction：`src/low_speed_av_control/src/stanley_controller.cpp:47` 至 `src/low_speed_av_control/src/stanley_controller.cpp:55`。

## Findings
### A2-CT-001：正常控制命令路径已实现
- Severity: P0
- Status: Pass by static audit / Not Verified by ROS2 runtime
- Impact on planning/control/vehicle operation: 收到有效 pose+trajectory 后，控制节点不再只停车，而会计算并发布控制命令。
- Recommended fix: 增加 ROS2 节点级测试和 C++ CLI smoke，直接调用 C++ 控制链路。
- Verification method: 注入 PoseStamped、Trajectory、VehicleState 后检查有限 `/control/command`。

### A2-CT-002：Pure Pursuit 和 Stanley 是真实实现
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 默认 pure_pursuit 与可选 stanley 可用于低速路径跟踪基础验证。
- Recommended fix: 增加曲线、反向、终点附近和低速测试。
- Verification method: C++ 单元测试或 offline C++ CLI。

### A2-CT-003：Limiter、Smoother、NaN/Inf guard 已接入
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 超限或非有限输出会被限制或转入安全停车。
- Recommended fix: 增加每个字段的 NaN/Inf 注入测试。
- Verification method: 构造 NaN steering/front/rear/brake 输入，检查 reason=`nan_or_inf_guard`。

### A2-CT-004：LQR/MPC sampler 仍是实验骨架
- Severity: P2
- Status: Partial
- Impact on planning/control/vehicle operation: 选择 LQR/MPC 时可能无法达到生产级跟踪效果，且部分 YAML 权重/horizon 未真正影响输出。
- Recommended fix: 接入专属 option struct、horizon 和 cost 权重，并明确 experimental 状态。
- Verification method: 改变 LQR/MPC 配置后输出应发生可预期变化。

### A2-CT-005：ROS2 runtime 未验证
- Severity: P1
- Status: Not Verified
- Impact on planning/control/vehicle operation: 可能存在 C++ 编译、消息字段、参数命名或 executor 时序问题。
- Recommended fix: 在真实 ROS2 环境运行 build/test/launch。
- Verification method: `colcon build/test` 和 topic/service 测试。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic pub /localization/pose ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic pub /planning/trajectory ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`
- SKIPPED_ROS2_UNAVAILABLE: `colcon build --packages-select low_speed_av_control`

