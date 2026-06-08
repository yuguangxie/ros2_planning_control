# 第二轮审计总览

## Objective
对优化后的低速自动驾驶 ROS2 规划/控制工程进行第二轮审计，比较第一轮 `docs/audits/` 中的 P0/P1/P2 问题是否已修复、部分修复或仍然遗留，并记录无 ROS2 环境下可执行的离线验证结果。

## Status: Partial
总体状态为部分通过。第一轮阻断级 P0 问题已经在源码层面得到明显修复：规划节点现在创建 `ReloadRoadnet`、`PlanRoute`、`SetPlannerAlgorithm` 服务并发布路线/轨迹；控制节点现在创建 `SetControllerAlgorithm` 服务，接入控制器工厂、车辆模型工厂、限幅、平滑和安全急停。  

仍未完全通过的主要原因是：本环境未运行 ROS2 编译、接口生成、launch 和 topic/service 运行时测试；C++ RoadnetLoader 仍未实现运行时 SHA-256 内容比对；语义区的 speed-zone/no-go 只被加载，尚未参与规划约束；LQR/MPC sampler 仍是可用骨架而非成熟控制器。

## Evidence
- 四个包存在：`src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_bringup`。
- 规划服务创建见 `src/low_speed_av_planning/src/planning_node.cpp:56`、`src/low_speed_av_planning/src/planning_node.cpp:63`、`src/low_speed_av_planning/src/planning_node.cpp:70`。
- 规划路线和轨迹发布见 `src/low_speed_av_planning/src/planning_node.cpp:355`、`src/low_speed_av_planning/src/planning_node.cpp:371`。
- 控制正常链路入口见 `src/low_speed_av_control/src/control_node.cpp:216`，控制器/车辆模型创建见 `src/low_speed_av_control/src/control_node.cpp:82` 至 `src/low_speed_av_control/src/control_node.cpp:83`。
- 安全急停订阅和优先级见 `src/low_speed_av_control/src/control_node.cpp:55` 至 `src/low_speed_av_control/src/control_node.cpp:56`，以及 `src/low_speed_av_control/src/control_node.cpp:192` 至 `src/low_speed_av_control/src/control_node.cpp:193`。
- Loader 使用 `project_manifest.json` 入口见 `src/low_speed_av_planning/src/roadnet_loader.cpp:115` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:117`。
- Loader 支持 `trajectory/waypoints.yaml` 和 `validation/validation_report.json`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:156`、`src/low_speed_av_planning/src/roadnet_loader.cpp:166`。
- `ControlCommand` 包含前/后轮转角，见 `src/low_speed_av_interfaces/msg/ControlCommand.msg:7` 至 `src/low_speed_av_interfaces/msg/ControlCommand.msg:8`。
- `/localization/pose` 默认且可配置，见 `src/low_speed_av_control/src/control_node.cpp:14`、`src/low_speed_av_control/config/control_params.yaml:12`。
- 优化最终报告记录已处理问题和限制，见 `reports/optimization_final_report.md`。

## Findings
| ID | Severity | Status | Finding | Impact on planning/control/vehicle operation | Recommended fix | Verification method |
|---|---|---|---|---|---|---|
| A2-SUM-001 | P0 | Pass by static audit, Not Verified by ROS2 runtime | 第一轮规划节点不产出 route/trajectory 的阻断问题已在源码层面修复。 | 规划模块现在具备从服务请求生成路线和轨迹的运行表面。 | 在真实 ROS2 环境运行 service call 和 topic echo。 | `ros2 service call`、`ros2 topic echo /planning/trajectory`；本地只做静态审计和 Python smoke。 |
| A2-SUM-002 | P0 | Pass by static audit, Not Verified by ROS2 runtime | 第一轮控制节点不产生正常跟踪命令的问题已在源码层面修复。 | 控制模块现在可以从 pose+trajectory 计算有限 Ackermann 命令。 | 增加节点级/组件级 ROS2 测试。 | ROS2 环境注入 PoseStamped 和 Trajectory 后检查 `/control/command`。 |
| A2-SUM-003 | P1 | Fixed | safety estop 已接入 `ModuleStatus` 并优先于正常输出。 | 外部安全状态可覆盖控制输出，降低车辆继续运动风险。 | 增加 estop latched/clear 策略文档和测试。 | 发布 `ModuleStatus(level>=2)` 后检查 reason=`safety_estop`。 |
| A2-SUM-004 | P1 | Partially Fixed | Loader 已结构化解析 validation 和边界，但运行时 SHA-256 比对仍未实现。 | 损坏或篡改包在 C++ runtime 中仍可能仅 warning，不会被拒绝。 | 实现 `checksums.sha256` 和 `manifest.hashes` 的 SHA-256 比对。 | 篡改 sample 文件后 loader 应拒绝加载。 |
| A2-SUM-005 | P2 | Partially Fixed | semantics 已加载并可用于 task/parking 目标解析，但 speed-zone/no-go 尚未影响规划。 | 规划可能忽略限速区、禁行区等语义约束。 | 将 semantic areas 转换为速度/禁行约束并接入 planner/speed planner。 | 构造 no-go/speed-zone 样例并断言 route/speed 改变。 |

## Offline checks
- `python scripts\validate_expected_tree.py`：失败，退出码 1，stdout 为空；判断为 Windows Python 占位符不可用。
- `py scripts\validate_expected_tree.py`：失败，`py` 命令不存在。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`：通过，输出 `Expected tree OK: .`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`：通过，输出 `AD Package OK: src\low_speed_av_bringup\sample_ad_package (3 nodes, 2 edges, 6 waypoints)`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`：通过，输出 route `['E_L001_F', 'E_L002_F']`、轨迹点 6、Pure Pursuit/Stanley 有限、Ackermann 有限、estop OK。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test-result --verbose`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /low_speed_av_planning/plan_route ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/trajectory`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`

