# 第三轮审计总览

## Objective（目标）
对当前 ROS2 低速自动驾驶规划与控制工程进行第三轮详细审计，覆盖四个 ROS2 包、样例 Low Speed Roadnet AD Package v1.1、离线脚本、配置、launch、报告和 ROS2 集成准备状态。本轮为审计模式，仅创建审计文档，不修改源码。

## Status（状态）
Partial。源码层面已补齐第二轮审计后的大部分安全与可信度缺口，包括 C++ SHA-256 校验逻辑、语义 speed_zone / no_go 约束、estop latch / clear 策略、LQR/MPC 可配置项和离线 smoke 脚本。但是当前 Windows 环境没有 ROS2、`colcon`、`ros2`，且默认 `python` 与 `py` 不可用，因此 ROS2 编译、launch、service、topic 运行时行为仍为 Not Verified。

## Evidence（证据）
- 工程包结构存在：`src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_bringup`。
- 接口生成列表完整：`src/low_speed_av_interfaces/CMakeLists.txt:8` 到 `src/low_speed_av_interfaces/CMakeLists.txt:19`。
- `ControlCommand` 包含前后轮转角：`src/low_speed_av_interfaces/msg/ControlCommand.msg:7` 到 `src/low_speed_av_interfaces/msg/ControlCommand.msg:8`。
- `RoadnetLoader` 使用 `project_manifest.json`：`src/low_speed_av_planning/src/roadnet_loader.cpp:213` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:215`。
- `RoadnetLoader` 支持 v1.1.x：`src/low_speed_av_planning/src/roadnet_loader.cpp:222` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:224`。
- 验证失败与 `blocking_errors` 拒绝：`src/low_speed_av_planning/src/roadnet_loader.cpp:253` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:265`。
- SHA-256 计算与 mismatch 抛错：`src/low_speed_av_planning/src/roadnet_loader.cpp:36`、`src/low_speed_av_planning/src/roadnet_loader.cpp:491` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:494`。
- planning 服务与发布器存在：`src/low_speed_av_planning/src/planning_node.cpp:72` 到 `src/low_speed_av_planning/src/planning_node.cpp:99`。
- control 正常跟踪、安全停车、estop 优先级存在：`src/low_speed_av_control/src/control_node.cpp:203`、`src/low_speed_av_control/src/control_node.cpp:227`、`src/low_speed_av_control/src/control_node.cpp:235` 到 `src/low_speed_av_control/src/control_node.cpp:259`。
- 默认定位话题为 `/localization/pose`：`src/low_speed_av_control/config/control_params.yaml:12`，`src/low_speed_av_bringup/config/control_params.yaml:8`。
- 运行离线检查的可用解释器：`C:\Program Files\FreeCAD 1.2\bin\python.exe`。
- 离线检查结果：`validate_expected_tree.py`、`validate_sample_ad_package.py`、`offline_algorithm_smoke.py`、`offline_remaining_fixes_smoke.py` 均通过。
- ROS2 环境脚本输出 `SKIPPED_ROS2_UNAVAILABLE`：`scripts/check_ros2_env.ps1:15`、`scripts/check_ros2_env.ps1:21`、`scripts/check_ros2_env.ps1:38`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-SUM-001 | P1 | Not Verified | ROS2 编译、测试、launch、service、topic 未在真实 ROS2 环境验证。 |
| A3-SUM-002 | P2 | Partial | C++ 核心逻辑有源码实现和 Python 离线镜像 smoke，但缺少已编译执行的 C++ gtest 或 CLI smoke 结果。 |
| A3-SUM-003 | P2 | Partial | 语义 speed_zone / no_go 已应用，但几何判断偏保守，主要基于参考点落入多边形，尚未做边段相交和车辆外廓膨胀。 |
| A3-SUM-004 | P2 | Partial | LQR 与 MPC sampler 已可配置且输出随配置变化，但仍明确属于实验控制器，不适合直接作为生产默认。 |
| A3-SUM-005 | P2 | Not Verified | `yaml-cpp` CMake 链接与系统依赖在目标 ROS2 发行版中尚未编译验证。 |
| A3-SUM-006 | P3 | Partial | Windows 无 ROS2 开发体验已有 PowerShell 检查脚本，但默认 `python` 和 `py` 不可用会影响直接执行体验。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
- 未验证 ROS2 编译和运行时可能导致节点无法启动、服务无法调用或 topic 类型不匹配，属于集成风险。
- 语义几何约束保守会降低误入禁行区风险，但也可能漏检穿越禁行区边界的长边段。
- LQR/MPC experimental 状态不会阻塞默认 `pure_pursuit`，但若用户切换到实验控制器，需要低速仿真和实车隔离验证。
- Python 离线 smoke 通过说明核心设计路径合理，但不能替代真实 C++ 编译和 ROS2 图验证。

## Recommended fix（推荐修复）
1. 在真实 ROS2 Humble/Iron/Jazzy 环境运行 `colcon build`、`colcon test`、launch、service 和 topic 验证。
2. 添加 C++ gtest 或轻量 CLI smoke target，直接链接 `low_speed_av_planning` 与 `low_speed_av_control` 库。
3. 将 no_go / keepout 约束升级为边段与多边形相交检测，并可选加入车辆 footprint 膨胀。
4. 保持 `pure_pursuit` 为默认控制器，将 LQR/MPC 标记为 experimental 并要求仿真验证。
5. 在 README 中补充 Windows 推荐解释器或脚本包装方式，避免默认 `python` 失败造成误解。

## Verification method（验证方法）
- 已执行：`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`，通过。
- 已执行：`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`，通过。
- 已执行：`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`，通过。
- 已执行：`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_remaining_fixes_smoke.py`，通过。
- 已执行：`powershell -ExecutionPolicy Bypass -File scripts\check_ros2_env.ps1`，正确输出跳过 ROS2 命令。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `colcon test`
- `colcon test-result --verbose`
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 service call /low_speed_av_planning/plan_route ...`
- `ros2 topic echo /planning/trajectory`
- `ros2 topic echo /control/command`
