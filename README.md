# Codex ROS2 Planning & Control Generation Pack — AD Package v1.1 对齐版

本包用于在 Codex Goal 模式中生成两个下游 ROS2 模块：

- `low_speed_av_planning`：读取路网编辑器导出的 Low Speed Roadnet AD Package ZIP，执行全局规划、局部轨迹拼接、速度规划和规划状态发布。
- `low_speed_av_control`：订阅规划轨迹、定位和底盘状态，执行可选控制算法，输出阿克曼底盘控制命令。

辅助包：

- `low_speed_av_interfaces`：只放 ROS2 `msg/srv/action`。
- `low_speed_av_bringup`：只放 launch、配置、demo 和离线检查脚本。

## 已对齐的路网 ZIP 协议

本版已经与当前路网编辑器 AD Package v1.1.0 对齐。Codex 生成的 loader 必须以 `project_manifest.json` 为唯一推荐入口，不能继续假设 `manifest.json`；必须读取 `trajectory/waypoints.yaml`，不能继续假设 `trajectory/waypoints.json`；必须读取 `validation/validation_report.json`，不能继续假设根目录 `validation_report.json`。

Canonical 文件：

```text
project_manifest.json
checksums.sha256
map/map_metadata.yaml
roadnet/roadnet.json
roadnet/topology.json
roadnet/route_graph.yaml
trajectory/waypoints.yaml
trajectory/waypoints.csv
trajectory/waypoint_index.json
semantics/areas.json
semantics/route_points.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
schemas/project_manifest.schema.json
schemas/roadnet.schema.json
schemas/topology.schema.json
schemas/waypoints.schema.json
schemas/waypoint_index.schema.json
schemas/semantics.schema.json
schemas/validation_report.schema.json
examples/mission.example.json
```

## Codex 环境约束

当前 Codex 运行环境没有 ROS2，因此不要要求 Codex 执行 `colcon build`、`ros2 launch` 或 `ros2 topic`。本包的 prompts 已改为：

1. 生成完整 ROS2 源码、CMake、package.xml、msg/srv/action、launch、config、docs、tests。
2. 通过纯 Python 离线脚本检查文件结构、AD Package schema、算法伪运行和配置。
3. 每个阶段输出 `reports/phase_xx_report.md`，说明完成内容、未运行的 ROS2 命令、替代验证结果和下一步。
4. 在最终报告中明确：ROS2 构建命令需要在真实 ROS2 Humble/Iron/Jazzy 环境中执行。

## 推荐使用方式

在目标仓库根目录解压本包，然后在 Codex Goal 模式中先发送：

```text
prompts/goal_00_master_goal.md
```

如果一次性任务过大，则按阶段发送：

```text
prompts/phase_00_discovery.md
prompts/phase_01_interfaces.md
prompts/phase_02_ad_package_loader.md
prompts/phase_03_global_planner.md
prompts/phase_04_motion_planner_speed_planner.md
prompts/phase_05_planning_node_integration.md
prompts/phase_06_vehicle_model_control_interfaces.md
prompts/phase_07_control_algorithms.md
prompts/phase_08_control_node_integration_safety.md
prompts/phase_09_bringup_config_docs.md
prompts/phase_10_tests_without_ros2_acceptance.md
prompts/phase_11_final_report.md
```

## 默认运行接口

定位输出 topic 默认是：

```yaml
topics.localization_pose_topic: "/localization/pose"
```

必须可在配置文件中改成其它 topic。

规划模块算法可选：

```yaml
global_planner.algorithm: "astar"        # dijkstra | astar
motion_planner.algorithm: "reference_line" # reference_line | stop_and_wait | frenet_lite | hybrid_astar_parking
speed_planner.algorithm: "curvature"      # constant | curvature | obstacle_aware
```

控制模块算法可选：

```yaml
controller.algorithm: "pure_pursuit"      # pure_pursuit | stanley | lqr | mpc_sampler
vehicle.model: "front_ackermann"          # front_ackermann | dual_ackermann
```

## 关键产物

- `AGENTS.md`：Codex 最高优先级工程约束。
- `skills/*/SKILL.md`：针对路网协议、ROS2 架构、规划、控制、无 ROS2 测试的分技能说明。
- `docs/*`：总体架构、数据契约、接口、算法、配置、测试和实施方案。
- `prompts/*`：Goal 模式主 prompt 和分阶段 prompt。
- `templates/sample_ad_package/*`：与当前路网编辑器 AD Package v1.1.0 对齐的最小样包目录。
- `templates/offline_validation/*`：Codex 在无 ROS2 环境中应生成/复用的离线验证脚本模板。
