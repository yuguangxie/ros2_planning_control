# 安全与控制命令第二轮审计

## Objective
审计 safety estop 优先级、timeout、空轨迹停车、限幅、平滑、NaN/Inf guard、前/后轮 Ackermann 输出和安全停车 reason。

## Status: Partial
源码层面 safety 和 command pipeline 已接通。ROS2 runtime 未验证，estop 清除/锁存策略仍需更明确的运行合同。

## Evidence
- safety topic 默认声明：`src/low_speed_av_control/src/control_node.cpp:17`。
- safety subscriber：`src/low_speed_av_control/src/control_node.cpp:55` 至 `src/low_speed_av_control/src/control_node.cpp:56`。
- estop 判断：`src/low_speed_av_control/src/control_node.cpp:151` 至 `src/low_speed_av_control/src/control_node.cpp:156`。
- estop 优先于 timeout/normal command：`src/low_speed_av_control/src/control_node.cpp:192` 至 `src/low_speed_av_control/src/control_node.cpp:193`。
- localization timeout：`src/low_speed_av_control/src/control_node.cpp:196` 至 `src/low_speed_av_control/src/control_node.cpp:200`。
- trajectory timeout：`src/low_speed_av_control/src/control_node.cpp:203` 至 `src/low_speed_av_control/src/control_node.cpp:207`。
- empty trajectory stop：`src/low_speed_av_control/src/control_node.cpp:210` 至 `src/low_speed_av_control/src/control_node.cpp:212`。
- front Ackermann 公式：`src/low_speed_av_control/src/front_ackermann_model.cpp:12` 至 `src/low_speed_av_control/src/front_ackermann_model.cpp:14`。
- dual Ackermann 公式：`src/low_speed_av_control/src/dual_ackermann_model.cpp:13` 至 `src/low_speed_av_control/src/dual_ackermann_model.cpp:20`。
- NaN/Inf guard：`src/low_speed_av_control/src/command_limiter.cpp:11` 至 `src/low_speed_av_control/src/command_limiter.cpp:16`。

## Findings
### A2-SAFE-001：safety estop 优先级正确
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 外部 safety error 可以覆盖正常轨迹跟踪，输出 controlled stop。
- Recommended fix: 明确 estop 是否 latched、如何恢复；增加状态机测试。
- Verification method: 连续发布 normal trajectory 和 estop，检查命令 reason 始终为 `safety_estop`。

### A2-SAFE-002：timeout 和空轨迹停车仍保留
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 定位/轨迹超时不会继续输出上一次运动命令。
- Recommended fix: 为 timeout 阈值加入边界测试。
- Verification method: 模拟时间推进，检查 `localization_timeout`、`trajectory_timeout`、`empty_trajectory`。

### A2-SAFE-003：前轮和双 Ackermann 输出符合公式
- Severity: P2
- Status: Pass
- Impact on planning/control/vehicle operation: 前后轮转角输出满足车型切换需求，支持四轮反相转向。
- Recommended fix: 增加曲率正负、rear ratio 边界和限幅测试。
- Verification method: 给定 kappa/wheel_base/rear_ratio，断言 front/rear steering 数值。

### A2-SAFE-004：NaN/Inf guard 已覆盖核心数值字段
- Severity: P2
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 非有限控制输出会被替换为 emergency stop，避免发布异常转角/速度。
- Recommended fix: 增加 fault injection 单元测试。
- Verification method: 各字段注入 NaN/Inf，断言 `enable=false`、`brake=1.0`、reason=`nan_or_inf_guard`。

### A2-SAFE-005：所有安全行为未在 ROS2 runtime 下验证
- Severity: P1
- Status: Not Verified
- Impact on planning/control/vehicle operation: 真实 topic 时序、QoS 和参数加载仍可能暴露问题。
- Recommended fix: 添加 ROS2 launch test 或 component test。
- Verification method: ROS2 环境中发布 safety/status 和 trajectory topic。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic pub /safety/status ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test --packages-select low_speed_av_control`

