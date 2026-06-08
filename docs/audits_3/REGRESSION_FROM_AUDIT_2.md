# 第二轮审计回归检查

## Objective（目标）
对照 `docs/audits_2/` 中的主要 P0/P1/P2 问题，确认当前代码是否已修复、部分修复、仍然打开、回归或无法验证。

## Status（状态）
Partial。第二轮审计中的主要源码缺口已有明显改善，没有发现新的 P0 回归。剩余问题集中在 ROS2 运行时未验证、缺少 C++ 编译级测试、语义几何约束成熟度和实验控制器成熟度。

## Evidence（证据）
- 第二轮风险要求补齐 checksum：当前有 `sha256_hex` 和 mismatch throw，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:36`、`src/low_speed_av_planning/src/roadnet_loader.cpp:491` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:494`。
- 第二轮要求语义约束：当前规划节点应用 speed_zone，见 `src/low_speed_av_planning/src/planning_node.cpp:252` 到 `src/low_speed_av_planning/src/planning_node.cpp:265`；loader 根据 no_go / keepout 标记 blocked_edges，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:434`。
- 第二轮要求 estop 策略：当前 control 参数包含 `estop_latched`，见 `src/low_speed_av_control/config/control_params.yaml:39`；代码实现 latch/clear，见 `src/low_speed_av_control/src/control_node.cpp:188` 到 `src/low_speed_av_control/src/control_node.cpp:199`。
- 第二轮要求 LQR/MPC 成熟度：当前 typed options 存在，见 `src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:21` 到 `src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:36`；实现使用配置，见 `src/low_speed_av_control/src/lqr_controller.cpp:43` 到 `src/low_speed_av_control/src/lqr_controller.cpp:55` 和 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:53` 到 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:79`。
- 第二轮要求安全 skeleton：`stop_and_wait` 输出停止轨迹，见 `src/low_speed_av_planning/src/stop_and_wait_motion_planner.cpp:19` 到 `src/low_speed_av_planning/src/stop_and_wait_motion_planner.cpp:20`。
- 离线 remaining fixes smoke 通过：`scripts/offline_remaining_fixes_smoke.py:230`。

## Findings（发现）
| Previous issue ID | Previous severity | Current status | Evidence | Remaining action |
|---|---|---|---|---|
| A2-SUM-001 规划节点运行管线 | P1 | Fixed | `planning_node.cpp:81` 到 `planning_node.cpp:99` 提供服务，`planning_node.cpp:386` 到 `planning_node.cpp:428` 生成 route/trajectory 并发布。 | 在 ROS2 环境调用服务验证。 |
| A2-SUM-002 控制正常命令路径 | P1 | Fixed | `control_node.cpp:203` 到 `control_node.cpp:227` 运行控制器、车辆模型、limiter、smoother。 | 在 ROS2 topic 流中验证时序和类型。 |
| A2-SUM-003 estop 集成 | P1 | Fixed | `control_node.cpp:188` 到 `control_node.cpp:199`，`control_node.cpp:235` 到 `control_node.cpp:236`。 | 在 ROS2 环境验证 latch/clear 行为。 |
| A2-SUM-004 checksum warning-only | P1 | Fixed / Not Verified by C++ runtime | `roadnet_loader.cpp:491` 到 `roadnet_loader.cpp:494` mismatch 抛异常。 | 用 C++ 测试或真实节点加载篡改包验证。 |
| A2-SUM-005 semantics 仅加载未约束 | P2 | Partially Fixed | `roadnet_loader.cpp:434` no_go/keepout blocked_edges；`planning_node.cpp:252` speed_zone 降速。 | 增加边段相交、footprint 膨胀和样例显式 no_go/speed_zone。 |
| A2-R-001 ROS2 集成未验证 | P1 | Still Open | `check_ros2_env.ps1` 输出 `SKIPPED_ROS2_UNAVAILABLE`。 | 在真实 ROS2 环境执行集成计划。 |
| A2-R-004 缺少 C++/CLI smoke | P2 | Partially Fixed | Python smoke 存在：`scripts/offline_remaining_fixes_smoke.py`；未发现 C++ gtest/CLI target。 | 添加编译级 C++ smoke。 |
| A2-R-005 LQR/MPC maturity | P2 | Partially Fixed | 配置项生效，但控制器 reason 标记 experimental。 | 仿真闭环验证，必要时限制生产使用。 |
| A2-R-006 estop latch/clear policy | P2 | Fixed / Runtime Not Verified | `control_node.cpp:188` 到 `control_node.cpp:199`。 | ROS2 topic/service 场景验证恢复策略。 |
| A2-R-007 motion planner skeleton safety | P2 | Partially Fixed | `stop_and_wait` 明确停车；frenet/hybrid 仍为 fallback/skeleton。 | 文档维持 experimental，并避免默认启用。 |
| A2-R-008 Windows no-ROS2 体验 | P3 | Partially Fixed | `scripts/check_ros2_env.ps1:15` 到 `scripts/check_ros2_env.ps1:38`。 | 补充 Python 包装脚本或 README 指引。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
当前没有发现第二轮之后的安全回归。主要风险来自真实 ROS2 环境未验证和语义几何约束精度不足，这些会影响集成可靠性和复杂地图下的路线安全性。

## Recommended fix（推荐修复）
- 优先在 ROS2 环境执行集成计划。
- 添加 C++ gtest/CLI smoke 直接覆盖 loader、planner、controller、vehicle model。
- 升级 semantics 几何约束并添加显式样例。

## Verification method（验证方法）
- 已用 FreeCAD Python 运行四个离线脚本并通过。
- 已静态检索第二轮问题对应源码。
- 未运行 ROS2 编译和 ROS2 graph 验证。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `colcon test`
- `ros2 launch`
- `ros2 service call`
- `ros2 topic pub`
- `ros2 topic echo`

