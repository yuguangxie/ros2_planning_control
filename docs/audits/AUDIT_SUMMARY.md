# 审计总览

## 目标
审计当前生成的低速自动驾驶 ROS2 规划/控制项目，覆盖项目结构、AD Package v1.1 兼容性、RoadnetLoader、接口、话题配置、规划算法、控制算法、车辆模型、安全逻辑、bringup、无 ROS2 离线验证、测试文档与维护风险。

## 总体状态
部分通过。

项目已经生成四个目标包，并且离线 Python 检查在可用解释器下通过。但 ROS2 节点运行链路尚未完整：规划节点目前只加载 roadnet 并发布状态，不提供路线规划服务，也不会发布真实规划路线/轨迹；控制节点有超时/空轨迹停车路径，但在收到有效 pose 和 trajectory 后不会调用控制器生成正常跟踪指令。

## 证据
- 四个包位于 `src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_bringup`，来自 `Get-ChildItem -Recurse`。
- `git status --short` 显示全仓库均为未跟踪文件；`git ls-files` 无输出。
- 接口生成清单见 `src/low_speed_av_interfaces/CMakeLists.txt:8` 至 `src/low_speed_av_interfaces/CMakeLists.txt:20`。
- RoadnetLoader 读取 `project_manifest.json`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:64`。
- RoadnetLoader 通过 manifest/fallback 读取 `trajectory/waypoints.yaml`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:131`。
- RoadnetLoader 通过 manifest/fallback 读取 `validation/validation_report.json`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:91`。
- `ControlCommand` 包含前/后轮转角字段，见 `src/low_speed_av_interfaces/msg/ControlCommand.msg:5` 和 `src/low_speed_av_interfaces/msg/ControlCommand.msg:6`。
- `/localization/pose` 默认配置在 `src/low_speed_av_planning/config/planning_params.yaml:10` 和 `src/low_speed_av_control/config/control_params.yaml:4`。

## 离线检查结果
状态：通过，但需要解释器兜底。

- `python scripts\validate_expected_tree.py`：失败，Windows Store Python 占位程序退出码 1，无有效 stdout。
- `py scripts\validate_expected_tree.py`：失败，未安装 `py` 命令。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`：通过，输出 `Expected tree OK: .`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`：通过，输出 `AD Package OK: src\low_speed_av_bringup\sample_ad_package (3 nodes, 2 edges, 6 waypoints)`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`：通过，route 为 `['E_L001_F', 'E_L002_F']`，轨迹点数 6，Pure Pursuit/Stanley 输出有限值。

## 主要发现
| ID | 严重级别 | 状态 | 发现 | 影响 |
|---|---|---|---|---|
| F-001 | P0 | 失败 | 规划节点未提供 route planning/reload 服务，也不会发布计算后的 `/planning/global_route` 或 `/planning/trajectory`。证据：`src/low_speed_av_planning/src/planning_node.cpp:17` 至 `src/low_speed_av_planning/src/planning_node.cpp:67` 只有 publisher/status/load 逻辑。 | 车辆运行时无法获得规划路线和轨迹。 |
| F-002 | P0 | 失败 | 控制节点在有效轨迹下没有实例化或调用控制器、车辆模型、限幅器、平滑器。证据：`src/low_speed_av_control/src/control_node.cpp:78` 至 `src/low_speed_av_control/src/control_node.cpp:95` 只处理超时和空轨迹停车。 | 车辆无法跟踪有效轨迹。 |
| F-003 | P1 | 失败 | RoadnetLoader 的 validation 和 checksum 处理过弱；validation 使用字符串搜索，C++ 不执行 SHA-256。证据：`src/low_speed_av_planning/src/roadnet_loader.cpp:93` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:99`，以及 `src/low_speed_av_planning/src/roadnet_loader.cpp:186` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:192`。 | 错误或被篡改的包可能被接受，合法包也可能被误拒。 |
| F-004 | P1 | 部分通过 | 安全参数存在，但未订阅/使用 safety estop；控制节点也未调用 limiter/smoother。证据：`src/low_speed_av_control/src/control_node.cpp:14` 只声明参数，实际订阅仅有 pose/trajectory/vehicle state，见 `src/low_speed_av_control/src/control_node.cpp:21` 至 `src/low_speed_av_control/src/control_node.cpp:29`。 | 外部安全状态无法通过控制节点强制停车。 |
| F-005 | P2 | 部分通过 | Bringup/launch 默认参数路径为空，未默认指向内置 sample package。证据：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:11` 和 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:12`。 | demo launch 默认启动后很可能不会加载 roadnet/config。 |

## 严重级别定义
- P0：阻断核心规划/控制运行，或可能导致不安全指令。
- P1：重大协议、安全或运行时缺口，集成前必须处理。
- P2：重要正确性、可维护性或可用性问题。
- P3：低风险改进、文档或测试补强项。

## 建议下一步
优先执行 `FIX_PLAN.md` 的 Phase 1 和 Phase 2：补齐规划节点 route/trajectory 服务与发布链路，并补齐控制节点正常控制指令链路，包括 ControllerFactory、VehicleModelFactory、CommandLimiter、CommandSmoother 和安全状态输入。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test-result --verbose`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/trajectory`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /control/command`
