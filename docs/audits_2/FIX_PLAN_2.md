# 第二轮修复计划

## Objective
基于第二轮审计结果，给出下一阶段修复路线。每个阶段包含目标、可能影响文件、精确修复说明、验收标准和建议 Codex prompt。

## Status: Pass
修复计划覆盖当前剩余 P1/P2 风险，并将 ROS2 runtime 验证与无 ROS2 可执行测试分开。

## Evidence
- 需要优先修复 checksum：`docs/audits_2/ROADNET_LOADER_AUDIT_2.md` 的 A2-RL-004，证据为 `src/low_speed_av_planning/src/roadnet_loader.cpp:327` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:334`。
- 需要补 C++ 测试：`docs/audits_2/TESTING_WITHOUT_ROS2_AUDIT_2.md` 的 A2-TST-003 和 A2-TST-004。
- 需要接入语义约束：`docs/audits_2/AD_PACKAGE_COMPATIBILITY_AUDIT_2.md` 的 A2-AD-005。
- 需要 ROS2 runtime 验证：`docs/audits_2/AUDIT_2_SUMMARY.md` 的 A2-SUM-001 和 A2-SUM-002。
- 需要提升 LQR/MPC：`docs/audits_2/CONTROL_MODULE_AUDIT_2.md` 的 A2-CT-004。

## Fix plan
| Phase | Target | Files likely affected | Exact fix description | Acceptance criteria | Suggested Codex prompt for that fix phase |
|---|---|---|---|---|---|
| 1 | C++ runtime checksum/hash 校验 | `src/low_speed_av_planning/src/roadnet_loader.cpp`、`roadnet_loader.hpp`、可选 `checksum_utils.*`、tests/scripts | 实现 SHA-256；解析 `checksums.sha256` 和 `manifest.hashes`；当 `roadnet.verify_checksums=true` 时 missing 可 warning，mismatch 必须拒载；错误信息包含文件路径。 | 合法 sample 加载通过；篡改 `trajectory/waypoints.yaml` 后 C++ loader 拒绝；报告不再写“Python validator performs SHA-256”作为 runtime 替代。 | “只修改 RoadnetLoader checksum 行为，实现 C++ SHA-256 校验 checksums.sha256 和 manifest.hashes，增加坏包负样例，不运行 ROS2。” |
| 2 | C++ 离线测试/CLI smoke | `src/low_speed_av_planning/test`、`src/low_speed_av_control/test`、`CMakeLists.txt`、`scripts/` | 增加可在有编译环境时运行的 C++ tests 或 CLI，覆盖 loader、Dijkstra/A*、reference_line、speed planners、Pure Pursuit、Stanley、Ackermann、limiter/smoother、estop helper。 | C++ 测试不依赖 ROS graph；能覆盖 failed validation、bad index、NaN guard、front/dual Ackermann。 | “为规划/控制核心类增加无 ROS graph 的 C++ 单元测试或 CLI smoke，保持 Windows 无 ROS2 环境可跳过并记录原因。” |
| 3 | 语义约束接入规划/速度 | `roadnet_types.hpp`、`roadnet_loader.cpp`、global planner options、speed planners、planning README/tests | 将 semantic areas 分类为 speed_zone/no_go/TODO；no_go 转 blocked edges 或 route filter；speed_zone 调整 target speed；task/parking/charging 目标解析补状态输出。 | 带 no-go 的样例 route 避让或失败；speed-zone 样例 target_speed 降低；语义计数发布到 status。 | “基于已加载 semantics，将 speed-zone/no-go 接入 speed/global planner，保留清晰 TODO 并增加离线样例测试。” |
| 4 | ROS2 集成验证准备 | `README.md`、`reports/`、可选 `scripts/check_ros2_env.ps1` | 编写 ROS2 环境检查脚本和集成测试清单；不在无 ROS2 环境声称成功；明确 build/test/launch/service/topic 顺序。 | 在无 ROS2 环境输出 SKIPPED；在 ROS2 环境可执行 colcon build/test 和 launch smoke。 | “添加 ROS2 环境检查和集成验证脚本，确保无 ROS2 时只记录 SKIPPED_ROS2_UNAVAILABLE，不修改运行逻辑。” |
| 5 | LQR/MPC sampler 成熟度 | `lqr_controller.cpp`、`mpc_sampler_controller.cpp`、`control_types.hpp`、`control_node.cpp`、`control_params.yaml`、README/tests | 增加算法专属 options；LQR 使用可调 gains 和曲率前馈；MPC sampler 使用配置 horizon/samples/cost weights；明确 experimental。 | 修改 LQR/MPC YAML 会改变输出；offline smoke 覆盖确定性输出。 | “改进 LQR/MPC sampler 配置接入和确定性测试，保持无 heavy solver，文档标明 experimental。” |
| 6 | Safety estop 状态机 | `control_node.cpp`、`control_node.hpp`、config/docs/tests | 明确 estop 是否锁存；如锁存，增加 clear 条件或服务；如非锁存，文档写明由 safety status 连续发布控制。 | estop 激活/解除行为可测试且文档一致。 | “为控制节点补充 safety estop latched/clear 策略和测试，不改变 ModuleStatus 输入合同。” |
| 7 | Motion planner skeleton 行为补强 | `stop_and_wait_motion_planner.*`、`frenet_lite_motion_planner.*`、`hybrid_astar_parking_planner.*`、README/tests | `stop_and_wait` 至少输出停车轨迹；frenet/hybrid_astar_parking 明确继承 reference_line 的 fallback 行为和 TODO。 | 四个 motion planner factory 都有可解释输出；选择 skeleton 不产生危险高速轨迹。 | “补强 motion planner skeleton 的安全输出和文档，特别是 stop_and_wait 停车轨迹。” |

## Findings
### A2-FIX-001
- Severity: P1
- Finding: 下一步应优先处理 checksum 和 ROS2 runtime 验证准备，而不是继续扩展新功能。
- Impact on planning/control/vehicle operation: 这两项决定能否安全、可信地进入集成环境。
- Recommended fix: 按 Phase 1、2、4 执行。
- Verification method: checksum 负样例、C++ smoke、ROS2 build/test 清单。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
