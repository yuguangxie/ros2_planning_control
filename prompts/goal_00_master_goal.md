# Goal 00 — Generate ROS2 Planning and Control Modules for Low Speed Roadnet AD Package v1.1

你现在在一个没有 ROS2 运行环境的 Codex 环境中工作。请不要把 `colcon build`、`ros2 launch`、`ros2 topic` 作为必须执行的验收命令。你仍然需要生成完整的 ROS2 源码、接口、CMake、package.xml、launch、config、文档、测试脚本和阶段报告。所有不能运行的 ROS2 命令必须写入报告的 `SKIPPED_ROS2_UNAVAILABLE` 部分。

## 背景

当前路网编辑器导出的正式自动驾驶输入包是 Low Speed Roadnet AD Package ZIP，schema 版本是 `1.1.0`。它不是 Nav2、Lanelet2，也不是前端 `.roadnet` 编辑工程文件。它面向 global planner、motion planner、controller、task manager 和 safety validation。

必须以如下文件作为 canonical contract：

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

请注意：旧设计中的 `manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json` 不是当前协议主路径。生成代码必须以 `project_manifest.json`、`trajectory/waypoints.yaml` 和 `validation/validation_report.json` 为准。

## 总目标

生成一个 ROS2 workspace 源码结构：

```text
src/
  low_speed_av_interfaces/
  low_speed_av_planning/
  low_speed_av_control/
  low_speed_av_bringup/
```

其中：

1. `low_speed_av_planning` 是完整规划模块。
2. `low_speed_av_control` 是完整控制模块。
3. 两者通过 ROS2 interfaces 解耦。
4. 定位话题默认 `/localization/pose`，必须可在配置文件中修改。
5. 规划模块必须有多个算法可选。
6. 控制模块必须有多个算法可选。
7. 底盘模型是四轮阿克曼，必须支持 `front_ackermann` 和 `dual_ackermann`。

## 必须实现的规划功能

### AD Package Loader

实现 `RoadnetLoader`：

- 读取 `project_manifest.json`。
- 校验 `schema == low_speed_roadnet_ad_package`。
- 支持 `schema_version == 1.1.0` 或 `1.1.x`。
- 读取 `coordinate_system` 和 `units`。
- 读取 manifest `files` 字段定位其它文件。
- 读取 `roadnet/topology.json`。
- 读取 `trajectory/waypoints.yaml`。
- 读取 `trajectory/waypoint_index.json`。
- 读取 `semantics/areas.json`、`task_points.json`、`parking_points.json`、`charging_points.json`。
- 读取 `validation/validation_report.json`。
- 如果 validation failed 或 blocking_errors > 0，拒绝加载。
- 校验 checksums，如果没有 checksum 则报告 warning。
- 支持 `waypoint_index` 的新旧切片字段：优先 `end_index_exclusive`，否则把 `end_index` 当 inclusive。
- 将 waypoint `kappa` 映射为内部 `kappa_1pm`，`v_mps` 映射为内部 `target_speed_mps`。

### Global Planner

实现算法可选：

```text
dijkstra
astar
```

要求：

- 从 topology nodes/edges 构建有向图。
- 使用 edge.cost。
- 支持 runtime blocked_edges。
- 支持 allow_reverse 配置。
- 输出 edge_id sequence、node_id sequence、length、estimated_time。

### Motion Planner

实现算法可选：

```text
reference_line
stop_and_wait
frenet_lite
hybrid_astar_parking
```

最低要求：

- `reference_line` 完整实现。
- 其它高级算法可以先实现可替换 skeleton，但必须有接口、工厂、配置和清晰 TODO，不允许空文件。
- 通过 waypoint_index + waypoints.yaml 拼接 edge sequence。
- 去除相邻 edge 重复点。
- 重新生成 route-level `s_m`。
- 根据当前定位裁剪 horizon。
- 发布 trajectory。

### Speed Planner

实现算法可选：

```text
constant
curvature
obstacle_aware
```

最低要求：

- constant 和 curvature 完整实现。
- obstacle_aware 可先实现保守 stub：如果未来障碍距离小于阈值则停车。

## 必须实现的控制功能

### Vehicle Models

支持：

```text
front_ackermann
dual_ackermann
```

Front Ackermann：

```text
kappa = tan(delta_front) / wheel_base
```

Dual Ackermann counter-phase：

```text
kappa = (tan(delta_front) - tan(delta_rear)) / wheel_base
```

用 `rear_steer_ratio` 生成后轮转角：

```text
tan(delta_front) = kappa * wheel_base / (1 + rear_steer_ratio)
delta_rear = atan(-rear_steer_ratio * tan(delta_front))
```

### Controller Algorithms

实现算法可选：

```text
pure_pursuit
stanley
lqr
mpc_sampler
```

最低要求：

- Pure Pursuit 完整实现。
- Stanley 完整实现。
- LQR 可使用可配置增益 + 曲率前馈的工程版 skeleton。
- MPC sampler 使用 deterministic sampling，不引入重型求解器依赖。

### Safety and Smoothing

必须实现：

- localization timeout。
- trajectory timeout。
- safety estop。
- empty trajectory stop。
- command limiter。
- command smoother。
- NaN/Inf command guard。

输出 `/control/command` 必须包含前轮和后轮转角字段，以支持前后双转阿克曼。

## 接口要求

生成 `low_speed_av_interfaces`，至少包含：

```text
msg/TrajectoryPoint.msg
msg/Trajectory.msg
msg/GlobalRoute.msg
msg/ControlCommand.msg
msg/VehicleState.msg
msg/ModuleStatus.msg
msg/RoadnetStatus.msg
srv/ReloadRoadnet.srv
srv/PlanRoute.srv
srv/SetPlannerAlgorithm.srv
srv/SetControllerAlgorithm.srv
```

## 配置要求

默认定位 topic：

```yaml
topics.localization_pose_topic: "/localization/pose"
```

规划配置：

```yaml
global_planner.algorithm: "astar"
motion_planner.algorithm: "reference_line"
speed_planner.algorithm: "curvature"
```

控制配置：

```yaml
controller.algorithm: "pure_pursuit"
vehicle.model: "front_ackermann"
```

## 无 ROS2 环境验收

生成以下脚本，并在当前环境能运行时运行：

```text
scripts/validate_expected_tree.py
scripts/validate_sample_ad_package.py
scripts/offline_algorithm_smoke.py
```

验收内容：

1. 文件结构完整。
2. sample AD Package 能加载。
3. `project_manifest.json` 路径正确。
4. `trajectory/waypoints.yaml` 能解析。
5. `validation/validation_report.json` 能解析。
6. topology 能规划 N0001 -> N0003。
7. waypoint_index 能拼接 trajectory。
8. Pure Pursuit 和 Stanley offline 命令有限值。

## 阶段报告

请按阶段实现，并每阶段创建报告：

```text
reports/phase_00_report.md
reports/phase_01_report.md
...
reports/phase_11_report.md
reports/final_generation_report.md
```

每个报告包含：目标、修改文件、设计决策、AD Package 兼容说明、topic/config 兼容说明、运行的离线检查、因 ROS2 不可用而跳过的命令、已知限制、下一阶段交接。

## 真实 ROS2 环境后续命令

最终报告必须写出但不要在当前环境强制运行：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

## 验收标准

- 生成完整四包 ROS2 workspace 源码。
- 规划和控制包边界清晰。
- RoadnetLoader 完全对齐 AD Package v1.1。
- `/localization/pose` 默认且可配置。
- 多规划算法、多控制算法、两种阿克曼车辆模型均有实现或合理 skeleton。
- 无 ROS2 环境中离线检查通过或给出明确跳过原因。
- 不出现“已经 colcon build 成功”的虚假报告。
