# low_speed_av_planning

## 模块定位
`low_speed_av_planning` 是低速自动驾驶的规划模块，负责读取 Low Speed Roadnet AD Package v1.1、构建拓扑图、执行全局规划、拼接参考轨迹并应用速度规划。

规划模块只负责生成路线和轨迹，不直接输出底盘控制命令。控制命令由 `low_speed_av_control` 负责。

## 输入与输出

### 输入
- AD Package v1.1 解压目录。
- 可选定位位姿话题，默认 `/localization/pose`。
- 规划服务请求，例如起点/终点 node id。

### 输出
- `/planning/global_route`：全局路线。
- `/planning/trajectory`：控制模块订阅的参考轨迹。
- `/planning/status`：规划模块状态。
- `/planning/roadnet_status`：roadnet 加载状态。

所有话题名都在 `config/planning_params.yaml` 中配置。

## AD Package v1.1 合同
模块以 `project_manifest.json` 为唯一推荐入口，支持：

```text
project_manifest.json
checksums.sha256
roadnet/topology.json
roadnet/roadnet.json
roadnet/route_graph.yaml
trajectory/waypoints.yaml
trajectory/waypoint_index.json
validation/validation_report.json
semantics/areas.json
semantics/route_points.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
```

旧路径不作为 primary input：

```text
manifest.json
trajectory/waypoints.json
validation_report.json
```

## 关键类说明

### RoadnetLoader
位置：

```text
include/low_speed_av_planning/roadnet_loader.hpp
src/roadnet_loader.cpp
```

职责：

- 读取 `project_manifest.json`。
- 校验 schema 和 schema_version。
- 通过 `manifest.files` 解析 topology、waypoints、waypoint_index、validation report。
- 将 `waypoints.yaml` 字段映射为内部字段：
  - `x` -> `x_m`
  - `y` -> `y_m`
  - `yaw` -> `yaw_rad`
  - `kappa` -> `kappa_1pm`
  - `v_mps` -> `target_speed_mps`
  - `s_m` -> `edge_s_m`
- 支持 `end_index_exclusive`，并兼容 legacy inclusive `end_index`。

当前注意事项：

- C++ runtime 会校验 `checksums.sha256` 和 `manifest.hashes`；checksum mismatch 会拒绝加载。
- 已加载 areas、route_points、task_points、parking_points、charging_points。
- `no_go_area`、`keepout` 或 `allow_planning_through=false` 的 area 会保守地屏蔽命中 waypoint 的边。
- `speed_zone` 或带 `speed_limit_mps` 的 area 会对命中 waypoint 进行二次限速。

### TopologyGraph
封装 topology nodes/edges，为 Dijkstra/A* 提供邻接边查询和几何启发式距离。

### GlobalPlannerFactory
支持：

- `dijkstra`
- `astar`

全局规划输出：

- edge id 序列。
- node id 序列。
- route length。
- estimated time。

### MotionPlannerFactory
支持：

- `reference_line`：已实现，按 edge sequence 拼接 waypoint。
- `stop_and_wait`：安全骨架，基于 reference-line 输出显式停车轨迹。
- `frenet_lite`：实验骨架，当前继承 reference-line 行为。
- `hybrid_astar_parking`：实验骨架，当前继承 reference-line 行为。

### SpeedPlannerFactory
支持：

- `constant`：固定速度。
- `curvature`：根据曲率和最大横向加速度限速。
- `obstacle_aware`：轻量 stub，障碍物距离过近时下游点停车。

## 配置说明
配置文件：

```text
config/planning_params.yaml
```

核心字段：

- `roadnet.package_path`：AD Package 根目录。
- `roadnet.reject_failed_validation`：validation 失败时拒绝加载。
- `topics.localization_pose_topic`：默认 `/localization/pose`。
- `global_planner.algorithm`：`dijkstra` 或 `astar`。
- `motion_planner.algorithm`：`reference_line` 等。
- `speed_planner.algorithm`：`constant`、`curvature`、`obstacle_aware`。

## 当前运行能力

当前节点已经实现：

- `ReloadRoadnet` service callback。
- `PlanRoute` service callback。
- `SetPlannerAlgorithm` service callback。
- 通过 GlobalPlannerFactory、MotionPlannerFactory、SpeedPlannerFactory 生成路线和轨迹。
- 发布 `GlobalRoute`、`Trajectory`、`ModuleStatus`、`RoadnetStatus`。
- 规划失败时发布 failure status 和 emergency stop trajectory。

仍需后续增强：

- 更完整的 ROS2 集成测试。
- 更完整的 C++ 单元测试和语义约束边界样例。

## 无 ROS2 验证
当前 Codex 环境可能没有 ROS2，可先运行：

```powershell
python scripts\validate_expected_tree.py
python scripts\validate_sample_ad_package.py
python scripts\offline_algorithm_smoke.py
python scripts\offline_remaining_fixes_smoke.py
```

如果 Windows `python` 是 Store 占位程序，可使用实际可用解释器，例如审计时使用过：

```powershell
& 'C:\Program Files\FreeCAD 1.2\bin\python.exe' scripts\offline_algorithm_smoke.py
```

## ROS2 环境验证
有 ROS2 后再运行：

```bash
colcon build --packages-select low_speed_av_interfaces low_speed_av_planning
colcon test --packages-select low_speed_av_planning
ros2 launch low_speed_av_planning planning.launch.py params:=/path/to/planning_params.yaml
```

## Production-linked tests

`test_roadnet_loader` 与 `test_planning_algorithms` 直接链接 `low_speed_av_planning` production library，覆盖当前 loader 合同、图搜索、轨迹拼接和速度规划行为。测试 fixture 使用临时目录，不修改正式 Roadnet 包。

当前 Windows 环境没有 ROS2/C++ 工具链，这些 gtest 已注册但状态为 `GENERATED_NOT_EXECUTED`；Python smoke 只承担数据合同和快速回归，不作为 C++ 行为证明。

## Phase 15 安全与可测试性补充

- `RoadnetLoader` 对 `manifest.files`、`manifest.hashes` 和 checksums 的实际读取路径使用统一 canonical containment；绝对路径、`..`、混合分隔符和 symlink escape 均 fail closed。
- Loader 拒绝重复 ID、未知引用、负数/非有限 cost、非法 waypoint 数值以及 index count/range/edge 不一致。
- 默认 A* 使用最小 `edge cost / endpoint distance` 缩放的 admissible heuristic；`heuristic_weight > 1` 明确报告 weighted A* 非最优模式。Dijkstra/A* 使用稳定 ID tie-break。
- `planning_helpers` 进入 production library，Node 与 `test_planning_helpers` 共用 semantic/current-pose anchor、terminal segment、route summary、连续性、semantic speed 与 progress window。
- local crop 保存 trajectory identity 与单调 progress，仅在有限窗口内结合 heading 匹配；reload、缓存清空和算法切换会 reset。

Phase 15 注册 `test_roadnet_loader`、`test_planning_algorithms`、`test_planning_helpers` 三个 production-linked gtest target，并注册 Planning-only launch test。当前 Windows 无 ROS2，新增 C++/launch 源码状态仍是 `GENERATED_NOT_EXECUTED` / `SKIPPED_ROS2_UNAVAILABLE`。
