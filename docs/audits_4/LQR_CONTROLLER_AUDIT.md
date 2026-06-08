# LQR 控制器审计

## Objective（目标）
审计升级后的 LQR 是否避免 Stanley fallback，是否使用 Riccati/DARE、曲率前馈、Q/R 配置、低速有限处理、预瞄点，并与统一曲率输出语义一致。

## Status（状态）
Partial。源码和离线 smoke 显示 LQR 为真实 Riccati 控制器，输出有限且配置敏感；但尚未通过 ROS2 runtime、C++ gtest、仿真闭环或真实底盘验证。

## Evidence（证据）
- LQR controller 没有 include 或调用 Stanley；`stanley` 仅在 `stanley_controller.cpp` 中出现，`lqr_controller.cpp` 未引用。
- 空轨迹和停车轨迹安全停车：`src/low_speed_av_control/src/lqr_controller.cpp:67` 到 `src/low_speed_av_control/src/lqr_controller.cpp:77`。
- 最近点和 preview：`src/low_speed_av_control/src/lqr_controller.cpp:36` 到 `src/low_speed_av_control/src/lqr_controller.cpp:44`，`src/low_speed_av_control/src/lqr_controller.cpp:84` 到 `src/low_speed_av_control/src/lqr_controller.cpp:85`。
- 低速 finite 建模速度：`src/low_speed_av_control/src/lqr_controller.cpp:82` 到 `src/low_speed_av_control/src/lqr_controller.cpp:84`。
- Q/R 配置：`src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:22` 到 `src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp:30`。
- Riccati 迭代：`src/low_speed_av_control/src/lqr_controller.cpp:100` 到 `src/low_speed_av_control/src/lqr_controller.cpp:134`。
- gain 和 feedforward 输出：`src/low_speed_av_control/src/lqr_controller.cpp:140` 到 `src/low_speed_av_control/src/lqr_controller.cpp:158`。
- 离线 LQR 测试：`scripts/offline_scu_lqr_smoke.py:202` 到 `scripts/offline_scu_lqr_smoke.py:224`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-LQR-001 | P3 | Pass | LQR 不再是 Stanley fallback，源码未发现 LQR 调用 Stanley。 |
| A4-LQR-002 | P3 | Pass | 实现了 2x2 离散 Riccati 迭代，无重型求解器依赖。 |
| A4-LQR-003 | P3 | Pass | 使用 `atan(wheel_base * kappa_ref)` 曲率前馈，可通过配置关闭。 |
| A4-LQR-004 | P3 | Pass | 使用最近点加 `preview_time_s` 选择参考点。 |
| A4-LQR-005 | P3 | Pass | 低速通过 `lqr_min_speed_mps` 保持模型有限，空/停轨迹返回安全停车。 |
| A4-LQR-006 | P2 | Partial | LQR 仍缺少真实 C++ gtest、仿真闭环指标和底盘级调参记录。 |
| A4-LQR-007 | P2 | Partial | LQR 输出先经 vehicle model、limiter、smoother，但该闭环路径未在 ROS2 runtime 验证。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
LQR 静态上已经是可用控制器形态，优于旧骨架实现。实车前仍必须进行仿真和 bench 验证，否则 Q/R 参数可能导致转向过激、响应不足或低速振荡。

## Recommended fix（推荐修复）
- 添加 C++ 单元测试覆盖左右偏差符号、曲线前馈、Q/R 变化、低速有限和 stop trajectory。
- 用仿真或数据回放验证 LQR 与 Pure Pursuit/Stanley 的横向误差收敛。
- 在真实底盘前限制 `max_steering_angle_rad` 和 SCU `max_target_speed_kmh`。

## Verification method（验证方法）
- 已执行 `offline_scu_lqr_smoke.py`，确认左右偏差符号相反、曲线前馈、Q/R 敏感、低速有限、双 Ackermann 有限。
- 静态核查 LQR 源码。
- 未执行 ROS2 runtime 或 C++ 编译测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon test --packages-select low_speed_av_control`
- `ros2 param set /low_speed_av_control controller.algorithm lqr`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`

