# 控制模块审计

## 目标
审计控制算法、车辆模型、指令限幅/平滑、安全停车和控制节点运行集成情况。

## 状态
部分通过。

## 证据
- ControllerFactory 支持全部请求算法，见 `src/low_speed_av_control/src/controller_factory.cpp:12` 至 `src/low_speed_av_control/src/controller_factory.cpp:26`。
- Pure Pursuit 的 nearest/target/steering 逻辑见 `src/low_speed_av_control/src/pure_pursuit_controller.cpp:40` 至 `src/low_speed_av_control/src/pure_pursuit_controller.cpp:58`。
- Stanley 的 heading/lateral correction 逻辑见 `src/low_speed_av_control/src/stanley_controller.cpp:45` 至 `src/low_speed_av_control/src/stanley_controller.cpp:57`。
- LQR 是 Stanley fallback 加曲率前馈骨架，见 `src/low_speed_av_control/src/lqr_controller.cpp:13` 至 `src/low_speed_av_control/src/lqr_controller.cpp:18`。
- MPC sampler 使用固定采样，见 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:23` 至 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:35`。
- 前轮 Ackermann 公式见 `src/low_speed_av_control/src/front_ackermann_model.cpp:12`。
- 双 Ackermann 反相后轮公式见 `src/low_speed_av_control/src/dual_ackermann_model.cpp:12` 至 `src/low_speed_av_control/src/dual_ackermann_model.cpp:17`。
- 控制节点 timeout/empty stop 逻辑见 `src/low_speed_av_control/src/control_node.cpp:78` 至 `src/low_speed_av_control/src/control_node.cpp:95`。

## 发现
### F-CT-001：Pure Pursuit 和 Stanley 是真实确定性实现
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：离线测试能得到有限控制输出。
- 推荐修复：补充 C++ 单元测试，并统一 Pure Pursuit 输出是曲率还是转角的语义，再与车辆模型集成。
- 验证方法：用直线和曲线路径构造期望输出测试。

### F-CT-002：控制节点缺少正常轨迹跟踪指令路径
- 严重级别：P0
- 状态：失败
- 对规划/控制/车辆运行影响：收到有效 pose 和 trajectory 后，timer 没有调用控制器，车辆不会收到持续跟踪指令。
- 推荐修复：在 `on_timer` 中实例化/缓存选定控制器和车辆模型，计算命令，完成转角/曲率转换，经过 limiter 和 smoother 后发布。
- 验证方法：节点级测试输入 pose/trajectory，断言 `/control/command` enable=true 且前后轮转角有限。

### F-CT-003：safety estop topic 未使用
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：`/safety/status` 无法强制控制模块停车。
- 推荐修复：增加 configurable safety status subscriber 和 latched estop 状态；estop 优先级高于所有正常控制输出。
- 验证方法：发布 safety estop，断言输出 controlled stop 且 reason 为 `safety_estop`。

### F-CT-004：Limiter 和 Smoother 未接入 ControlNode
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：即使后续加入控制器输出，也不会自动执行限幅和平滑。
- 推荐修复：所有命令路径，包括停车路径，都调用 `CommandLimiter::limit` 和 `CommandSmoother::smooth`。
- 验证方法：注入超限速度/转角，输出应被限幅和平滑。

### F-CT-005：NaN/Inf guard 不完整
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：`front_steering_angle_rad`、`rear_steering_angle_rad`、acceleration、brake 等字段未逐项检查非有限值。
- 推荐修复：对所有数值指令字段执行 `std::isfinite`。
- 验证方法：分别向每个数值字段注入 NaN，断言进入 emergency stop。

### F-CT-006：LQR/MPC skeleton 未使用配置参数
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：`control_params.yaml` 中 LQR/MPC 的 gain、horizon、samples 不会影响算法输出。
- 推荐修复：扩展 `ControllerOptions` 或算法专属 option struct；读取配置，并在代码中明确 TODO。
- 验证方法：修改 LQR/MPC 配置后输出应发生变化。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic pub /planning/trajectory ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`
