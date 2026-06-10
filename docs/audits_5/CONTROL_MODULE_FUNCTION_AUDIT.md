# Control Module Function Audit

## Objective

审计控制模块的订阅、控制周期、控制器、车辆模型、限幅、平滑、finite guard 和状态输出。

## Scope

- `src/low_speed_av_control/src`
- `src/low_speed_av_control/include`
- `src/low_speed_av_control/config/control_params.yaml`

## Status

Pass by static/offline audit, Not Verified by ROS2 runtime.

## Evidence

- 订阅定位/轨迹/车辆状态/安全状态：`src/low_speed_av_control/src/control_node.cpp:83` 到 `src/low_speed_av_control/src/control_node.cpp:94`。
- 发布 internal command、SCU command、status：`src/low_speed_av_control/src/control_node.cpp:95` 到 `src/low_speed_av_control/src/control_node.cpp:99`。
- 控制器和车辆参数：`src/low_speed_av_control/src/control_node.cpp:28` 到 `src/low_speed_av_control/src/control_node.cpp:67`。
- LQR 参数：`src/low_speed_av_control/src/control_node.cpp:48` 到 `src/low_speed_av_control/src/control_node.cpp:56`。
- timeout/estop control cycle：`src/low_speed_av_control/src/control_node.cpp:285` 到 `src/low_speed_av_control/src/control_node.cpp:303`。
- Finite guard 在 limiter：`src/low_speed_av_control/src/command_limiter.cpp:11` 到 `src/low_speed_av_control/src/command_limiter.cpp:17`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-CTRL-001 | P3 | Pass | 控制节点订阅所需输入并发布 SCU 输出。 | 控制链路完整。 | 无。 | ROS2 topic list/echo。 |
| AUD5-CTRL-002 | P3 | Pass | estop、定位超时、轨迹超时均走 controlled stop。 | 安全基础路径存在。 | 无。 | 人工安全验证 N。 |
| AUD5-CTRL-003 | P3 | Pass | Pure Pursuit、Stanley、LQR、MPC sampler 通过工厂/参数选择。 | 多控制器可用。 | 无。 | 参数切换和 command echo。 |
| AUD5-CTRL-004 | P2 | Partial | 控制算法输出只通过离线 replica/smoke 验证，未注入真实 ROS2 msg 验证。 | 序列化、时间戳、QoS 仍可能有问题。 | 增加 ROS2 integration tests 或 launch smoke。 | 发布 PoseStamped + Trajectory。 |
| AUD5-CTRL-005 | P2 | Partial | LQR 可用但未在真实车/仿真闭环调参。 | 实车跟踪质量未知。 | 低速路径逐步调 Q/R 和限幅。 | H/O 节人工验证。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py`
- `uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 topic echo /control/status`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`
- `ros2 param set /low_speed_av_control controller.algorithm lqr`

## Remaining Uncertainty

真实 ROS2 控制周期、topic timing、车辆状态输入和底盘响应未验证。

