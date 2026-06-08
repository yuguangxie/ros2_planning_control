# 修复计划

## 目标
给出按阶段推进的修复路线，确保每一阶段都有目标、可能影响文件、精确修复说明、验收标准和建议 Codex prompt。

## 状态
通过。

## 修复阶段
| 阶段 | 目标 | 可能影响文件 | 精确修复说明 | 验收标准 | 建议 Codex prompt |
|---|---|---|---|---|---|
| 1 | 规划节点路线和轨迹运行链路 | `src/low_speed_av_planning/src/planning_node.cpp`、`planning_node.hpp`、planner factories、interface srv callbacks | 增加 `ReloadRoadnet`、`PlanRoute`、`SetPlannerAlgorithm` service server。构建 `TopologyGraph`，运行选定全局规划器，拼接 reference trajectory，应用速度规划，发布 `GlobalRoute`、`Trajectory`、`ModuleStatus`，错误时发布失败状态和停车轨迹。 | 调用 `PlanRoute N0001 -> N0003` 后发布非空 route 和 trajectory；无路线时发布失败状态和 emergency stop trajectory。 | “实现 low_speed_av_planning 节点服务集成，支持 ReloadRoadnet、PlanRoute、SetPlannerAlgorithm，并发布 route/trajectory/status；保持算法逻辑可在 ROS2 外测试。” |
| 2 | 控制节点正常指令路径 | `src/low_speed_av_control/src/control_node.cpp`、`control_node.hpp`、command limiter/smoother、controller options | 加载 controller/vehicle/smoother/limit 参数，实例化 factories，在有效 pose/trajectory 下计算控制命令，统一曲率/转角语义，应用 limiter 和 smoother 后发布。 | 有效 pose 和 trajectory 产生 enable=true 且有限的 `/control/command`；timeout/empty 仍停车。 | “实现 low_speed_av_control 正常跟踪闭环，使用 ControllerFactory、VehicleModelFactory、CommandLimiter、CommandSmoother、timeout guard 和 YAML 参数。” |
| 3 | 安全急停集成 | `control_node.cpp`、`control_node.hpp`、interfaces 如需要、docs/config | 定义 safety status 输入类型，订阅可配置 safety topic，维护 latched estop 状态，estop 激活时覆盖所有正常控制输出。 | 发布 estop status 后立即输出 reason=`safety_estop` 的停车指令。 | “为 low_speed_av_control 增加可配置 safety status subscription 和 estop override，不改变 package 边界。” |
| 4 | RoadnetLoader 结构化解析与 checksum | `roadnet_loader.cpp`、`roadnet_loader.hpp`、`roadnet_types.hpp`、tests/scripts | 用结构化解析替代字符串搜索；校验 manifest validation 和 validation report；实现 `checksums.sha256` 与 `manifest.hashes` 的 SHA-256；增加边界和字段错误信息。 | failed validation 和 checksum mismatch 被拒绝；合法 sample 可加载；legacy `end_index` 可用。 | “加固 RoadnetLoader：结构化解析 AD Package v1.1、校验 manifest/report、实现 SHA-256、补 range checks 和负样例测试。” |
| 5 | Semantics 加载 | `roadnet_types.hpp`、`roadnet_loader.cpp`、planning algorithms/config | 增加 areas、route/task/parking/charging points 数据结构；通过 manifest files 加载；支持 mission/task/parking 目标解析。 | loader 输出 semantics 计数；`PlanRoute` 可使用 task/parking point id。 | “为 low_speed_av_planning 增加 AD Package semantics 加载和 task/parking 目标解析。” |
| 6 | Launch/config 可用性 | `src/low_speed_av_bringup/launch/*.py`、config YAML、README | 使用 package share 默认路径，设置 sample package path，自动传入 config 文件，文档说明 override 参数。 | ROS2 环境中无参数运行 demo launch 后 sample roadnet ready。 | “修复 bringup launch 默认值，使其使用安装后的 config 和 sample AD package。” |
| 7 | C++ 离线/单元测试 | `tests/`、`CMakeLists.txt`、可选 CLI test target | 增加不依赖 ROS graph runtime 的 RoadnetLoader、Dijkstra/A*、reference_line、speed planner、车辆模型、控制器、limiter/smoother 测试。 | 有编译器和依赖时，C++ 逻辑测试可在无 ROS2 graph 的情况下通过。 | “为规划/控制算法类增加纯 C++ 单元测试或 CLI smoke target，不要求 ROS2 runtime。” |
| 8 | LQR/MPC skeleton 成熟度 | `lqr_controller.cpp`、`mpc_sampler_controller.cpp`、controller options/config docs | 接入 LQR/MPC 配置，添加清晰 TODO，改进 deterministic sampler cost，使用 horizon 和轨迹采样。 | 修改 LQR/MPC 配置会改变输出；文档清楚标明成熟度。 | “改进 LQR 和 MPC sampler skeleton，接入配置、补 TODO、加确定性测试和限制说明。” |

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
