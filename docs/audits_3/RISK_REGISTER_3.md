# 第三轮风险登记

## Objective（目标）
登记第三轮审计发现的剩余安全、集成、测试和维护风险，并给出优先级。

## Status（状态）
Partial。未发现 P0 风险；存在一个 P1 ROS2 运行时未验证风险，以及若干 P2 测试和几何成熟度风险。

## Evidence（证据）
- ROS2 unavailable：`scripts/check_ros2_env.ps1:15`、`scripts/check_ros2_env.ps1:21`。
- C++ checksum 仅静态确认：`src/low_speed_av_planning/src/roadnet_loader.cpp:491` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:494`。
- semantics no_go 点检测：`src/low_speed_av_planning/src/roadnet_loader.cpp:434`。
- LQR/MPC experimental 输出：`src/low_speed_av_control/src/lqr_controller.cpp:55`、`src/low_speed_av_control/src/mpc_sampler_controller.cpp:79`。
- C++ tests 未发现，当前依赖 Python smoke：`scripts/offline_remaining_fixes_smoke.py:230`。

## Risk Register（风险登记）
| ID | Title | Severity | Probability | Affected module | Evidence | Impact | Recommended mitigation | Priority |
|---|---|---|---|---|---|---|---|---|
| A3-R-001 | ROS2 运行时未验证 | P1 | Medium | all packages | `check_ros2_env.ps1` 输出 skipped | 可能构建失败、节点无法启动、topic/service 不兼容 | 在真实 ROS2 环境执行集成计划并建立 CI | High |
| A3-R-002 | 缺少 C++ 编译级 smoke/gtest | P2 | Medium | planning/control | 仅 Python smoke 通过 | C++ include、链接、字段名错误可能漏检 | 添加 C++ gtest 或 CLI smoke target | High |
| A3-R-003 | no_go/keepout 几何检测不完整 | P2 | Medium | planning | `roadnet_loader.cpp:434` | 长边段穿越禁行区可能漏检 | 实现 segment-polygon intersection 和 footprint 膨胀 | High |
| A3-R-004 | LQR/MPC 仍为 experimental | P2 | Medium | control | `lqr_controller.cpp:55`、`mpc_sampler_controller.cpp:79` | 切换实验控制器可能跟踪不稳定 | 保持非默认，增加仿真闭环测试 | Medium |
| A3-R-005 | yaml-cpp 构建兼容性未知 | P2 | Medium | planning | `src/low_speed_av_planning/CMakeLists.txt:35` | 目标 ROS2 环境可能链接失败 | 真实 colcon 构建后调整 target 或 rosdep | Medium |
| A3-R-006 | 默认 Windows Python 不可用 | P3 | High | scripts/docs | `python` exit 1，`py` not recognized | 新用户离线检查体验差 | 增加 PowerShell wrapper 自动发现解释器 | Medium |
| A3-R-007 | sample 缺少显式 no_go/speed_zone | P3 | Medium | bringup/sample | `areas.json` 主要为 drivable_area | 人工检查语义约束不直观 | 增加显式语义样例并更新 checksum | Low |

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-RG-001 | P1 | Open | 最高优先级是 ROS2 runtime 验证。 |
| A3-RG-002 | P2 | Open | C++ 测试和语义几何成熟度是下一轮最值得修复的源码/测试问题。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
风险主要影响从源码到真实 ROS2 运行的可信度，以及复杂语义地图下的路线安全性。默认低速规划控制链路风险已明显降低，但仍不能直接替代 ROS2 集成验证和仿真验证。

## Recommended fix（推荐修复）
按照 `FIX_PLAN_3.md` 的阶段顺序处理：先 ROS2 构建和 C++ smoke，再语义几何增强，最后完善 experimental controller 验证。

## Verification method（验证方法）
- 风险来自静态审计、离线脚本结果和 ROS2 unavailable 记录。
- 关闭风险需要真实 ROS2 验证或新增编译级测试证据。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `colcon test`
- `ros2 launch`
- `ros2 service call`
- `ros2 topic echo`

