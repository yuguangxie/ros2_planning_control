# 第二轮风险登记表

## Objective
记录优化后仍影响规划、控制、车辆运行和维护的主要风险，并给出优先级和缓解建议。

## Status: Partial
P0 运行链路风险已显著下降；剩余最高优先级风险集中在 ROS2 runtime 未验证、C++ checksum 未实现、语义约束未使用和 C++ 测试不足。

## Evidence
- ROS2 命令未运行并列入跳过项：`reports/optimization_final_report.md`。
- C++ checksum warning-only 行为：`src/low_speed_av_planning/src/roadnet_loader.cpp:327` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:334`。
- semantics 加载但未见约束接入：`src/low_speed_av_planning/src/roadnet_loader.cpp:287` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:312`。
- LQR fallback：`src/low_speed_av_control/src/lqr_controller.cpp:14` 至 `src/low_speed_av_control/src/lqr_controller.cpp:18`。
- MPC 固定采样：`src/low_speed_av_control/src/mpc_sampler_controller.cpp:25`。
- estop 当前状态设置：`src/low_speed_av_control/src/control_node.cpp:151` 至 `src/low_speed_av_control/src/control_node.cpp:156`。

## Risk register
| ID | Title | Severity | Probability | Affected module | Evidence | Impact | Recommended mitigation | Priority |
|---|---|---|---|---|---|---|---|---|
| A2-R-001 | ROS2 编译和运行时未验证 | P1 | 高 | 全工程 | 所有 ROS2 命令均在 `SKIPPED_ROS2_UNAVAILABLE`；仅静态/离线 Python 检查通过。 | CMake、接口、launch、参数、QoS 问题可能在集成时暴露。 | 在 ROS2 环境执行 build/test/launch/service/topic 全链路验证。 | 1 |
| A2-R-002 | C++ runtime checksum 未执行 SHA-256 比对 | P1 | 中 | RoadnetLoader | `roadnet_loader.cpp:327` 至 `:334` 只读取 checksums 并写 warning。 | 损坏或篡改 AD Package 可能被规划节点使用。 | 实现 `checksums.sha256` 与 `manifest.hashes` 比对，mismatch fatal。 | 2 |
| A2-R-003 | 语义区未转为规划/速度约束 | P2 | 中 | Planning | semantics 加载见 `roadnet_loader.cpp:287` 至 `:312`，但无 speed-zone/no-go 使用证据。 | 车辆可能规划穿过禁行区域或忽略限速语义。 | 接入 semantic areas 到 global planner/speed planner。 | 3 |
| A2-R-004 | C++ 逻辑测试不足 | P2 | 高 | Planning/Control | 离线脚本为 Python smoke，C++ 节点和算法未编译运行。 | C++ 回归可能通过 Python 检查。 | 增加 gtest 或 C++ CLI smoke target。 | 4 |
| A2-R-005 | LQR/MPC sampler 易被误用为成熟控制器 | P2 | 中 | Control | LQR fallback：`lqr_controller.cpp:14` 至 `:18`；MPC 固定 samples：`mpc_sampler_controller.cpp:25`。 | 跟踪效果不稳定，配置参数可能不生效。 | 标记 experimental，接入配置，增加测试。 | 5 |
| A2-R-006 | Safety estop 清除/锁存策略不够明确 | P2 | 中 | Control safety | `on_safety_status` 根据当前消息设置 bool，见 `control_node.cpp:151` 至 `:156`。 | 安全系统恢复时的控制恢复语义可能不一致。 | 明确 latched/clear service 或状态转换策略。 | 6 |
| A2-R-007 | Motion planner skeleton 仍可能被选中 | P3 | 中 | Planning | motion factory 支持 skeleton 算法，见 `reference_line_motion_planner.cpp:73` 至 `:84`。 | 输出行为可能低于用户预期。 | 文档标注成熟度，`stop_and_wait` 输出明确停车轨迹。 | 7 |
| A2-R-008 | 默认 Windows `python/py` 不可用 | P3 | 高 | Scripts/docs | `python` 退出 1 空输出；`py` 未安装。 | 离线检查入口体验差。 | 提供 PowerShell wrapper 自动选择可用解释器。 | 8 |

## Findings
### A2-RISK-001
- Severity: P1
- Finding: 第二轮没有新的 P0 blocker，但 P1 runtime 未验证和 checksum 未实现必须在集成前处理。
- Impact on planning/control/vehicle operation: 影响工程能否安全接入真实车辆或仿真。
- Recommended fix: 下一 Codex 目标聚焦 ROS2 环境验证准备、checksum 和 C++ tests。
- Verification method: ROS2 build/test + checksum 负样例。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
