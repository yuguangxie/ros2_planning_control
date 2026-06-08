# CODEX_PLANNING_CONTROL_GENERATION_GUIDE.md

## 1. 目标

使用 Codex Goal 模式生成两个完整的下游自动驾驶 ROS2 模块：

```text
low_speed_av_planning
low_speed_av_control
```

这两个模块以路网编辑器导出的 Low Speed Roadnet AD Package ZIP 为输入数据源，服务低速无人车的全局规划、局部轨迹和循迹控制。规划和控制必须独立成包，接口通过 ROS2 topic/service/action 解耦。

## 2. 当前 AD Package 对齐要求

必须以当前 ZIP 协议为准：

```text
project_manifest.json
roadnet/topology.json
trajectory/waypoints.yaml
trajectory/waypoint_index.json
semantics/areas.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
```

旧包中提到的 `manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json` 只能作为兼容项，不能作为主协议。

## 3. Codex Goal 模式推荐输入

首次运行直接发送：

```text
prompts/goal_00_master_goal.md
```

该 prompt 会要求 Codex 生成完整工作区、源代码、配置、launch、测试和阶段报告。由于环境没有 ROS2，它不会强制运行 `colcon build`。

## 4. 分阶段运行

如果一次性 goal 过大，按以下顺序发送：

1. `phase_00_discovery.md`：仓库扫描与实施计划。
2. `phase_01_interfaces.md`：生成接口包。
3. `phase_02_ad_package_loader.md`：实现 AD Package loader 和离线校验。
4. `phase_03_global_planner.md`：实现 Dijkstra/A*。
5. `phase_04_motion_planner_speed_planner.md`：实现轨迹拼接和速度规划。
6. `phase_05_planning_node_integration.md`：实现规划 ROS2 node。
7. `phase_06_vehicle_model_control_interfaces.md`：实现阿克曼车辆模型和控制基础结构。
8. `phase_07_control_algorithms.md`：实现 Pure Pursuit、Stanley、LQR、MPC sampler。
9. `phase_08_control_node_integration_safety.md`：实现控制 ROS2 node、安全停和 smoother。
10. `phase_09_bringup_config_docs.md`：生成 bringup、配置和文档。
11. `phase_10_tests_without_ros2_acceptance.md`：无 ROS2 环境验收。
12. `phase_11_final_report.md`：最终报告和真实 ROS2 环境后续命令。

每个阶段都必须写 `reports/phase_xx_report.md`。

## 5. 离线验收方式

Codex 环境没有 ROS2 时，使用以下替代验收：

```bash
python3 scripts/validate_expected_tree.py
python3 scripts/validate_sample_ad_package.py
python3 scripts/offline_algorithm_smoke.py
```

这些脚本不替代真实 ROS2 构建，只用于确认文件结构、AD Package 契约和算法逻辑没有明显断裂。

## 6. 真实 ROS2 环境后续命令

在有 ROS2 的目标机器中再执行：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

## 7. 成功标准

生成结果至少应满足：

- 可以加载 `templates/sample_ad_package`。
- 可以从 topology 规划 edge 序列。
- 可以由 `waypoint_index + waypoints.yaml` 拼接局部 trajectory。
- 可以切换规划算法和控制算法。
- 控制模块默认订阅 `/localization/pose`，并可通过 config 修改。
- 控制模块支持 `front_ackermann` 和 `dual_ackermann`。
- Pure Pursuit、Stanley、LQR、MPC sampler 都能从 trajectory 取到需要字段。
- 无 ROS2 环境下有清晰的跳过记录和最终报告。
