# 风险登记表

## 目标
按优先级记录影响规划、控制、车辆运行和后续维护的主要风险。

## 状态
部分通过。

## 风险列表
| ID | 标题 | 严重级别 | 概率 | 受影响模块 | 证据 | 影响 | 推荐缓解措施 | 优先级 |
|---|---|---|---|---|---|---|---|---|
| R-001 | 规划节点无法产出 route/trajectory | P0 | 高 | Planning | `src/low_speed_av_planning/src/planning_node.cpp:17` 至 `src/low_speed_av_planning/src/planning_node.cpp:67` 只加载并发布状态。 | 车辆运行时收不到规划轨迹。 | 实现规划服务并发布计算后的 route/trajectory。 | 1 |
| R-002 | 控制节点无法跟踪有效轨迹 | P0 | 高 | Control | `src/low_speed_av_control/src/control_node.cpp:78` 至 `src/low_speed_av_control/src/control_node.cpp:95` 只处理停车分支。 | 车辆收不到正常跟踪指令。 | 增加 controller/vehicle/limiter/smoother 正常指令链路。 | 2 |
| R-003 | 安全急停被忽略 | P1 | 高 | 控制安全 | `src/low_speed_av_control/src/control_node.cpp:14` 声明 safety topic 参数，但无 subscriber。 | 外部安全系统无法使车辆停车。 | 实现 safety status subscriber 和优先级覆盖。 | 3 |
| R-004 | Loader 的 validation 可能误判 | P1 | 中 | 规划加载器 | 字符串 validation 检查见 `src/low_speed_av_planning/src/roadnet_loader.cpp:93` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:99`。 | 失败包可能被加载，合法包可能被拒绝。 | 结构化解析 JSON/YAML 并增加负样例测试。 | 4 |
| R-005 | 运行时 loader 不校验 checksum | P1 | 中 | 规划加载器 | `verify_checksums` 只写 warning，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:186` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:192`。 | 损坏或篡改 roadnet 可能被使用。 | 实现 `checksums.sha256` 与 `manifest.hashes` 校验。 | 5 |
| R-006 | 配置项未生效 | P1 | 高 | 规划/控制节点 | YAML 中有大量参数，但节点只声明少量参数，未构造完整 options。 | 使用者可能误以为安全/规划配置已生效。 | 将配置加载到 typed runtime options 并逐项测试。 | 6 |
| R-007 | Demo launch 默认无法加载配置和 sample roadnet | P2 | 高 | Bringup | `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:11` 和 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:12` 默认空参数。 | demo 默认启动后 planning inactive/waiting。 | 使用 package share 默认配置和 sample package 路径。 | 7 |
| R-008 | semantics 未被使用 | P2 | 中 | Planning loader/planners | `RoadnetPackage` 和 loader 中没有 semantics 数据模型。 | no-go、speed zone、parking/task semantics 无法影响规划。 | 增加 semantics model 和 planner 使用点。 | 8 |
| R-009 | 离线测试未覆盖 C++ 实现 | P2 | 中 | 测试 | Python smoke 自己实现规划/控制逻辑，见 `scripts/offline_algorithm_smoke.py:23` 至 `scripts/offline_algorithm_smoke.py:90`。 | C++ 回归可能通过离线检查。 | 增加 C++ 算法测试或 CLI smoke。 | 9 |
| R-010 | LQR/MPC 骨架可能被误认为生产控制器 | P3 | 中 | 控制算法 | LQR fallback 见 `src/low_speed_av_control/src/lqr_controller.cpp:13`；MPC 固定采样见 `src/low_speed_av_control/src/mpc_sampler_controller.cpp:23`。 | 如果被选用，跟踪效果可能不足。 | 标记 experimental/TODO，补配置接入和测试。 | 10 |

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
