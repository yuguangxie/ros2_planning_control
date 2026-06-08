# 控制模块审计 3

## Objective（目标）
审计 control 包的控制器工厂、车辆模型工厂、正常跟踪命令路径、限幅、平滑、NaN/Inf guard、timeout 与算法配置成熟度。

## Status（状态）
Partial。默认 `pure_pursuit` 和 `front_ackermann` 正常路径源码完整，Stanley、LQR、MPC sampler 也有实现；但控制节点未在 ROS2 topic 流中运行验证，LQR/MPC 仍是实验控制器。

## Evidence（证据）
- control 参数声明：`src/low_speed_av_control/src/control_node.cpp:16` 到 `src/low_speed_av_control/src/control_node.cpp:60`。
- subscriptions、publisher、SetControllerAlgorithm service：`src/low_speed_av_control/src/control_node.cpp:65` 到 `src/low_speed_av_control/src/control_node.cpp:84`。
- controller/vehicle/safety/runtime options 加载：`src/low_speed_av_control/src/control_node.cpp:99` 到 `src/low_speed_av_control/src/control_node.cpp:140`。
- 正常跟踪命令路径：`src/low_speed_av_control/src/control_node.cpp:203` 到 `src/low_speed_av_control/src/control_node.cpp:223`。
- limiter/smoother/finiteness guard 调用：`src/low_speed_av_control/src/control_node.cpp:227`。
- Pure Pursuit 实现：`src/low_speed_av_control/src/pure_pursuit_controller.cpp:41` 到 `src/low_speed_av_control/src/pure_pursuit_controller.cpp:57`。
- Stanley 实现：`src/low_speed_av_control/src/stanley_controller.cpp:47` 到 `src/low_speed_av_control/src/stanley_controller.cpp:55`。
- LQR experimental 实现：`src/low_speed_av_control/src/lqr_controller.cpp:43` 到 `src/low_speed_av_control/src/lqr_controller.cpp:55`。
- MPC sampler deterministic 实现：`src/low_speed_av_control/src/mpc_sampler_controller.cpp:53` 到 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:79`。
- Controller typed options：`src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:21` 到 `src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:36`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-CT-001 | P3 | Pass | `pure_pursuit` 和 `stanley` 是真实实现，不是空 stub。 |
| A3-CT-002 | P3 | Pass | `front_ackermann` 和 `dual_ackermann` 通过 factory 支持。 |
| A3-CT-003 | P3 | Pass | 正常控制命令会经过 limiter、smoother 和 finite guard。 |
| A3-CT-004 | P2 | Partial | LQR/MPC sampler 可配置，但仍为 experimental，缺少闭环仿真和实车约束验证。 |
| A3-CT-005 | P2 | Not Verified | topic 订阅、定时器、时间戳 timeout 与 ROS2 参数加载未在运行时验证。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
默认控制链路可以从有效 pose 和 trajectory 生成有限命令。实验控制器若未经验证使用，可能产生跟踪性能不足或振荡风险。ROS2 未验证会影响消息接收、时间戳和实际命令发布。

## Recommended fix（推荐修复）
- 在 ROS2 环境播放 pose/trajectory 并 echo `/control/command`。
- 增加 C++ controller 单元测试，覆盖 Pure Pursuit、Stanley、LQR、MPC 配置改变输出。
- 将 LQR/MPC 保持非默认并在配置注释中强调 experimental。

## Verification method（验证方法）
- Python offline smoke 输出 `pp=(0.500,0.000)`、`stanley=(0.500,0.000)`、Ackermann finite、estop OK。
- 静态确认源码路径完整。
- 未运行 ROS2 节点。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped ...`
- `ros2 topic pub /planning/trajectory low_speed_av_interfaces/msg/Trajectory ...`
- `ros2 topic echo /control/command`
- `ros2 service call /control/set_controller_algorithm ...`

