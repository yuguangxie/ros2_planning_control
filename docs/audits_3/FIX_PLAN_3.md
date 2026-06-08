# 第三轮后续修复计划

## Objective（目标）
给出第三轮审计后的分阶段修复计划，重点关闭 ROS2 运行时未验证、C++ 测试缺口、语义几何约束和实验控制器成熟度问题。

## Status（状态）
Pass。计划覆盖所有 P1/P2 剩余风险，并保持“不在无 ROS2 环境虚报成功”的原则。

## Evidence（证据）
- ROS2 集成计划已有基础：`docs/ROS2_INTEGRATION_TEST_PLAN.md:19` 到 `docs/ROS2_INTEGRATION_TEST_PLAN.md:59`。
- 当前脚本可清晰跳过 ROS2：`scripts/check_ros2_env.ps1:15`、`scripts/check_ros2_env.ps1:21`、`scripts/check_ros2_env.ps1:38`。
- C++ 核心库已拆分，适合添加 gtest/CLI target：`src/low_speed_av_planning/CMakeLists.txt:15`、`src/low_speed_av_control/CMakeLists.txt:14`。
- 语义约束当前简化实现：`src/low_speed_av_planning/src/roadnet_loader.cpp:434`、`src/low_speed_av_planning/src/planning_node.cpp:252` 到 `src/low_speed_av_planning/src/planning_node.cpp:265`。

## Fix Plan（修复计划）
| Phase | Target | Files likely affected | Exact fix description | Acceptance criteria | Suggested Codex prompt for that fix phase |
|---|---|---|---|---|---|
| 1 | ROS2 集成验证 | `docs/ROS2_INTEGRATION_TEST_PLAN.md`, CI/Docker files | 在真实 ROS2 环境执行 build/test/launch/service/topic，并记录结果；若失败，只修正构建和接口问题。 | `colcon build/test` 真实通过，launch 可启动，PlanRoute 可触发 trajectory，control 可发布 command。 | “在 ROS2 环境中执行集成计划，修复构建、launch、topic/service 问题，不改变 AD Package 合同。” |
| 2 | C++ smoke/gtest | `src/low_speed_av_planning/CMakeLists.txt`, `src/low_speed_av_control/CMakeLists.txt`, `test/*.cpp` | 添加 loader、planner、controller、vehicle、limiter 的 C++ 测试或 CLI smoke target。 | 不依赖 ROS graph 的 C++ 测试可编译运行，覆盖 checksum mismatch、bad index、route、trajectory、finite commands。 | “为 planning/control 核心库添加 C++ gtest/CLI smoke，覆盖第三轮审计 A3-R-002。” |
| 3 | 语义几何增强 | `roadnet_loader.cpp`, `roadnet_types.hpp`, sample semantics | 实现 segment-polygon intersection，必要时加入 footprint 膨胀；增加显式 no_go/speed_zone 样例并更新 checksum。 | 穿越 no_go 的边会被阻断，speed_zone 降速在 sample 中可复现。 | “增强 no_go/keepout/speed_zone 几何约束，补充样例和离线测试。” |
| 4 | Windows 离线体验 | `scripts/run_offline_checks.ps1`, `README.md` | 增加 PowerShell wrapper 自动寻找 `python`、`py`、已知解释器；输出清晰失败原因。 | 新用户执行一个脚本即可跑完离线检查或获得明确 SKIPPED 原因。 | “新增 Windows 离线检查包装脚本，自动发现 Python 并运行所有 no-ROS2 smoke。” |
| 5 | LQR/MPC 验证成熟度 | `lqr_controller.cpp`, `mpc_sampler_controller.cpp`, config/docs/tests | 保持 experimental 标识，添加配置敏感 C++ 测试和仿真建议；必要时限制最大输出变化。 | 配置改变输出的测试通过，文档明确不可作为生产默认。 | “完善 LQR/MPC experimental 验证与文档，不把它们改成默认控制器。” |

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-FP-001 | P1 | Planned | ROS2 runtime 验证是下一步最高优先级。 |
| A3-FP-002 | P2 | Planned | C++ 编译级测试是关闭源码可信度缺口的关键。 |
| A3-FP-003 | P2 | Planned | 语义几何增强是提升车辆运行安全性的关键。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
执行该计划后，可以把当前“源码和 Python 离线通过”提升到“C++ 编译级和 ROS2 集成可验证”，并降低复杂地图语义约束漏检风险。

## Recommended fix（推荐修复）
按 Phase 1 到 Phase 3 优先推进。Phase 4 和 Phase 5 可并行，但不要替代 ROS2 集成验证。

## Verification method（验证方法）
- 每一阶段均需要更新报告并记录命令输出。
- Phase 1 必须在真实 ROS2 环境执行。
- Phase 2/3 可先在无 ROS2 环境用 C++/Python smoke 补充静态验证。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- 本环境未执行 Phase 1 中的任何 ROS2 命令。

