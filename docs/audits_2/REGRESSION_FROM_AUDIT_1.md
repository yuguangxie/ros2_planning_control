# 第一轮审计回归对照

## Objective
对照 `docs/audits/` 中第一轮审计问题，判断当前优化后工程中每个主要问题的状态：Fixed、Partially Fixed、Still Open、Regressed 或 Not Verified。

## Status: Partial
多数 P0/P1 功能缺口已经在源码层面修复。由于当前环境没有 ROS2，节点运行、接口生成和 launch 实测只能标记为 Not Verified by ROS2 runtime。checksum、语义约束和 C++ 测试覆盖仍然是主要遗留项。

## Evidence
- 第一轮总览问题来源：`docs/audits/AUDIT_SUMMARY.md` 中 F-001 至 F-005。
- 第一轮规划问题来源：`docs/audits/PLANNING_MODULE_AUDIT.md` 中 F-PL-002 至 F-PL-005。
- 第一轮控制问题来源：`docs/audits/CONTROL_MODULE_AUDIT.md` 中 F-CT-002 至 F-CT-006。
- 第一轮 loader/AD Package 问题来源：`docs/audits/ROADNET_LOADER_AUDIT.md` 与 `docs/audits/AD_PACKAGE_COMPATIBILITY_AUDIT.md`。
- 当前源码证据集中在 `src/low_speed_av_planning/src/planning_node.cpp`、`src/low_speed_av_control/src/control_node.cpp`、`src/low_speed_av_planning/src/roadnet_loader.cpp`。
- 优化阶段自述来源：`reports/optimization_final_report.md`。

## Regression table
| Original audit issue ID | Original severity | Current status | Evidence | Remaining action |
|---|---|---|---|---|
| F-001 | P0 | Partially Fixed | 规划节点创建三类服务：`planning_node.cpp:56`、`:63`、`:70`；发布 route/trajectory：`:355`、`:371`。 | 在 ROS2 环境实际调用 service 并检查 topic。 |
| F-002 | P0 | Partially Fixed | 控制节点创建控制器/车辆模型：`control_node.cpp:82` 至 `:83`；正常路径发布 `compute_tracking_command()`：`:216`。 | 运行节点级测试，确认有效 pose+trajectory 产生有限 `/control/command`。 |
| F-003 | P1 | Partially Fixed | validation 已结构化解析：`roadnet_loader.cpp:150` 至 `:162`；checksum 仍只报告 warning：`:327` 至 `:334`。 | 实现 C++ SHA-256 校验。 |
| F-004 | P1 | Fixed by static audit | safety subscriber 见 `control_node.cpp:55` 至 `:56`，estop 优先级见 `control_node.cpp:192` 至 `:193`。 | 增加 ROS2 runtime estop 测试。 |
| F-005 | P2 | Fixed by static audit | bringup 默认 sample 包路径见 `planning_control_demo.launch.py:25` 至 `:31`，传参见 `:39`。 | ROS2 环境无参数 launch 验证。 |
| F-PL-002 | P0 | Partially Fixed | `PlanRoute` callback 见 `planning_node.cpp:337` 至 `:378`，失败时发布 failure/status 并发布停车轨迹见 `:347`、`:378`、`:379`。 | ROS2 service call 验证失败/成功分支。 |
| F-PL-003 | P1 | Partially Fixed | global/motion/speed 算法参数声明见 `planning_node.cpp:28` 至 `:39`，工厂使用见 `:207`、`:216`、`:218`。 | 补行为测试，证明修改 YAML 会改变输出。 |
| F-PL-004 | P3 | Still Open | `stop_and_wait` 仍是低成熟度骨架，相关 skeleton 行为未扩展为完整停车轨迹。 | 让 `stop_and_wait` 输出明确停车轨迹并增加测试。 |
| F-PL-005 | P3 | Still Open | `obstacle_aware` 仍为阈值停车 stub，见 `obstacle_aware_speed_planner.cpp:7` 至 `:29`。 | 接入障碍物输入合同。 |
| F-CT-002 | P0 | Partially Fixed | `compute_tracking_command` 见 `control_node.cpp:160`，timer 正常分支见 `control_node.cpp:216`。 | ROS2 runtime 注入消息验证。 |
| F-CT-003 | P1 | Fixed by static audit | `on_safety_status` 见 `control_node.cpp:151` 至 `:156`。 | 增加 estop clear/latched 策略测试。 |
| F-CT-004 | P1 | Fixed by static audit | `finalize_command` 调用 limiter/smoother，见 `control_node.cpp:184` 至 `:189`。 | 注入超限命令做节点级测试。 |
| F-CT-005 | P2 | Fixed by static audit | `CommandLimiter` 检查 speed、accel、steering、front/rear steering、brake，见 `command_limiter.cpp:11` 至 `:16`。 | 增加每字段 NaN/Inf 单元测试。 |
| F-CT-006 | P2 | Still Open | LQR 使用 Stanley fallback，见 `lqr_controller.cpp:14` 至 `:18`；MPC 使用固定 samples，见 `mpc_sampler_controller.cpp:25`。 | 接入 LQR/MPC 专属配置和测试。 |
| F-RL-002 | P1 | Fixed by static audit | 字符串搜索已替换为 `YAML::Node` 结构化读取，典型证据见 `roadnet_loader.cpp:120` 至 `:162`。 | 增加负样例测试。 |
| F-RL-003 | P1 | Fixed by static audit | manifest validation 和 validation report 均检查 `status/blocking_errors`，见 `roadnet_loader.cpp:148` 至 `:162`。 | 增加 failed report 负样例。 |
| F-RL-004 | P1 | Still Open | `verify_checksums` 未比对摘要，只读取并写 warning，见 `roadnet_loader.cpp:327` 至 `:334`。 | 实现 SHA-256。 |
| F-RL-005 | P2 | Partially Fixed | waypoint 有限值和 index bounds 检查见 `roadnet_loader.cpp:245` 至 `:280`。 | 增加 C++ 负样例测试。 |
| F-AD-002 | P2 | Partially Fixed | semantics 容器见 `roadnet_types.hpp:98` 至 `:102`，加载见 `roadnet_loader.cpp:287` 至 `:312`。 | 将语义约束接入规划/速度。 |
| F-AD-003 | P1 | Fixed by static audit | manifest validation 结构化检查见 `roadnet_loader.cpp:148` 至 `:152`。 | 增加 manifest/report 不一致负样例。 |
| F-AD-004 | P1 | Still Open | 同 F-RL-004，checksum 未做 C++ runtime 摘要比对。 | 实现 SHA-256 并设置失败策略。 |
| F-IT-003 | P1 | Partially Fixed | planning/control service server 均已创建，见 `planning_node.cpp:56`、`:63`、`:70` 和 `control_node.cpp:61`。 | ROS2 `service list` 未验证。 |
| F-IT-004 | P2 | Fixed by static audit | safety status 使用 `ModuleStatus`，消息定义见 `ModuleStatus.msg:1` 至 `:5`，订阅见 `control_node.cpp:55`。 | 补接口说明和 runtime 测试。 |
| F-CL-002 | P2 | Fixed by static audit | planning/control launch 使用 `FindPackageShare`，见 `planning.launch.py:5`、`:12`，`control.launch.py:5`、`:12`。 | ROS2 launch 实测。 |
| F-CL-003 | P2 | Fixed by static audit | bringup demo 默认 sample AD Package，见 `planning_control_demo.launch.py:25` 至 `:31`。 | ROS2 launch 实测 roadnet ready。 |
| F-CL-004 | P1 | Partially Fixed | 多数核心参数已被节点声明/读取，见 `planning_node.cpp:20` 至 `:39`、`control_node.cpp:14` 至 `:39`；LQR/MPC 专属配置仍未消费。 | 补全 LQR/MPC 和语义配置行为测试。 |

## Findings
### A2-REG-001
- Severity: P0
- Finding: 第一轮两个 P0 阻断问题在源码层面已转为可运行设计，但 ROS2 runtime 未验证。
- Impact on planning/control/vehicle operation: 如果 C++ 编译或 ROS2 参数命名存在问题，运行时仍可能失败。
- Recommended fix: 在 ROS2 环境执行 build/test/launch/service/topic 验证。
- Verification method: `colcon build`、`ros2 service call`、`ros2 topic echo`。

### A2-REG-002
- Severity: P1
- Finding: checksum 和 LQR/MPC 成熟度仍是最主要未解决 P1/P2 风险。
- Impact on planning/control/vehicle operation: 完整性校验不足会影响路网可信度；实验控制器误用会影响轨迹跟踪质量。
- Recommended fix: 下一阶段优先实现 runtime SHA-256，再做控制器成熟度和 C++ 测试。
- Verification method: 篡改包文件负样例、控制器配置行为测试。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service list`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /low_speed_av_planning/plan_route ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/trajectory`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`
