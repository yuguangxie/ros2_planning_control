# 规划模块代码走读

包：`src/low_speed_av_planning`

职责：加载 Low Speed Roadnet AD Package v1.1，构建拓扑图，执行全局规划、运动规划和速度规划，并发布路线、轨迹和状态。

## 1. 主节点

主节点类：`low_speed_av_planning::PlanningNode`

主文件：

- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`

节点名：`low_speed_av_planning`

证据：`src/low_speed_av_planning/src/planning_node.cpp:42`。

## 2. 参数声明

`PlanningNode` 在构造函数声明参数：

| 行号 | 参数 | 作用 |
|---:|---|---|
| 45 | `roadnet.package_path` | AD Package 根目录 |
| 46 | `roadnet.reject_failed_validation` | validation 失败时拒绝 |
| 47 | `roadnet.verify_checksums` | 启用 checksum |
| 48-52 | `topics.*` | localization、route、trajectory、status topic 名 |
| 53-56 | `global_planner.*` | 全局规划算法、启发式、reverse、blocked edges |
| 57-60 | `motion_planner.*` | 运动规划算法、horizon、拼接选项 |
| 61-66 | `speed_planner.*` | 速度规划算法、速度上限、障碍停车参数 |

## 3. Publishers

| Publisher | 默认 topic | 类型 | 证据 |
|---|---|---|---|
| `global_route_pub_` | `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | `planning_node.cpp:72` |
| `trajectory_pub_` | `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | `planning_node.cpp:74` |
| `planning_status_pub_` | `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | `planning_node.cpp:76` |
| `roadnet_status_pub_` | `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | `planning_node.cpp:78` |

## 4. Subscribers

当前 planning node 没有创建 subscription。`topics.localization_pose_topic` 参数存在，但当前 `compute_trajectory` 调用传入 `nullptr`，没有按实时 pose crop。证据：`src/low_speed_av_planning/src/planning_node.cpp:244`。

## 5. Service servers

| Service | 类型 | 作用 | 证据 |
|---|---|---|---|
| `/low_speed_av_planning/reload_roadnet` | `low_speed_av_interfaces/srv/ReloadRoadnet` | 重新加载 AD Package | `planning_node.cpp:81` |
| `/low_speed_av_planning/plan_route` | `low_speed_av_interfaces/srv/PlanRoute` | 触发路线规划 | `planning_node.cpp:88` |
| `/low_speed_av_planning/set_planner_algorithm` | `low_speed_av_interfaces/srv/SetPlannerAlgorithm` | 运行时切换算法 | `planning_node.cpp:95` |

## 6. RoadnetLoader 流程

入口：`RoadnetLoader::load`，证据：`src/low_speed_av_planning/src/roadnet_loader.cpp:208`。

步骤：

1. 要求 `project_manifest.json` 存在。
   - 证据：`roadnet_loader.cpp:213`。
2. 检查 `schema == low_speed_roadnet_ad_package`。
3. 支持 `schema_version` 为 `1.1.0` 或 `1.1.x`。
4. 读取 `package_id`、坐标系、`manifest.files`、`manifest.hashes`、`units`。
5. 检查 manifest validation。
6. 通过 `manifest.files` 或 canonical fallback 读取 `validation/validation_report.json`。
   - 证据：`roadnet_loader.cpp:259`。
7. validation report 失败或 blocking errors 大于 0 时拒绝加载。
8. 读取 `roadnet/topology.json`、`trajectory/waypoints.yaml`、`trajectory/waypoint_index.json`。
   - 证据：`roadnet_loader.cpp:268` 至 `roadnet_loader.cpp:270`。
9. 校验 topology node/edge 必填字段、edge 引用 node、数值有限。
10. 读取 waypoint，并映射：
    - `x -> x_m`
    - `y -> y_m`
    - `yaw -> yaw_rad`
    - `kappa -> kappa_1pm`
    - `v_mps -> target_speed_mps`
    - `s_m -> edge_s_m`
    - `direction -> gear`
    - 证据：`roadnet_loader.cpp:342`、`roadnet_loader.cpp:343`。
11. 读取 `waypoint_index`。优先 `end_index_exclusive`；缺失时兼容 inclusive `end_index + 1`。
    - 证据：`roadnet_loader.cpp:372` 至 `roadnet_loader.cpp:375`。
12. 加载 `areas`、`route_points`、`task_points`、`parking_points`、`charging_points`。
    - 证据：`roadnet_loader.cpp:390` 至 `roadnet_loader.cpp:426`。
13. `no_go_area` 或 `keepout` 覆盖 waypoint 时，将对应 edge 加入 `blocked_edges`。
    - 证据：`roadnet_loader.cpp:428` 至 `roadnet_loader.cpp:434`。
14. 启用 checksum 时验证 `checksums.sha256` 与 `manifest.hashes`。
    - 证据：`roadnet_loader.cpp:439`、`roadnet_loader.cpp:452`。

## 7. 拓扑图与全局规划

`TopologyGraph` 从 `RoadnetPackage` 中建立 node、edge 和 adjacency。

证据：

- `src/low_speed_av_planning/src/topology_graph.cpp:7` 构建图。
- `src/low_speed_av_planning/src/topology_graph.cpp:27` 返回 outgoing edges。
- `src/low_speed_av_planning/src/topology_graph.cpp:33` 计算 A* heuristic distance。

### Dijkstra

文件：`src/low_speed_av_planning/src/dijkstra_planner.cpp`

行为：

- 跳过 disabled、blocked_by_default、runtime blocked edges。
- 若 `allow_reverse=false`，跳过 reverse edge。
- 使用 edge.cost 搜索。
- 输出 node_ids、edge_ids、length、estimated_time。

证据：`dijkstra_planner.cpp:8`、`dijkstra_planner.cpp:36`、`dijkstra_planner.cpp:70`。

### A*

文件：`src/low_speed_av_planning/src/astar_planner.cpp`

行为：

- 与 Dijkstra 类似处理 blocked/reverse。
- 增加几何启发式 `heuristic_weight * graph.heuristic_distance`。

证据：`astar_planner.cpp:21`、`astar_planner.cpp:36`、`astar_planner.cpp:42`。

## 8. 运动规划

工厂入口：`MotionPlannerFactory::create`，证据：`src/low_speed_av_planning/src/reference_line_motion_planner.cpp:73`。

支持：

- `reference_line`
- `stop_and_wait`
- `frenet_lite`
- `hybrid_astar_parking`

### reference_line

`ReferenceLineMotionPlanner::make_trajectory` 按 route edge 顺序读取 waypoint index，拼接 waypoint，去重边界点，重算 route `s_m`，按 horizon 裁剪。

证据：

- `reference_line_motion_planner.cpp:19` 按 edge 拼接。
- `reference_line_motion_planner.cpp:28` 去重边界点。
- `reference_line_motion_planner.cpp:50` 重算 route `s_m`。
- `reference_line_motion_planner.cpp:58` horizon 裁剪。

### stop_and_wait

`stop_and_wait` 基于 reference line 输出停车轨迹，把速度置为 0 并标记 behavior。证据：`src/low_speed_av_planning/src/stop_and_wait_motion_planner.cpp:20`。

### frenet_lite / hybrid_astar_parking

当前作为骨架/保守 fallback 算法存在。操作上可被选择，但应按审计和 README 的限制说明，仅用于后续扩展或低风险测试。

## 9. 速度规划

工厂入口：`SpeedPlannerFactory::create`，证据：`src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:21`。

支持：

- `constant`
- `curvature`
- `obstacle_aware`

`curvature` 根据曲率和最大横向加速度限制速度。证据：`src/low_speed_av_planning/src/curvature_speed_planner.cpp:13`、`src/low_speed_av_planning/src/curvature_speed_planner.cpp:17`。

`obstacle_aware` 先应用 curvature，再根据障碍距离停车或降速。证据：`src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:7`。

`speed_zone` 语义在 planning node 中二次作用于 trajectory，限制区域内速度。证据：`src/low_speed_av_planning/src/planning_node.cpp:252`。

## 10. PlanRoute 回调步骤

入口：`PlanningNode::on_plan_route`，证据：`src/low_speed_av_planning/src/planning_node.cpp:385`。

步骤：

1. 解析起点和终点。
   - `start_node_id` 优先；否则可用 `start_task_point_id`。
   - `goal_node_id` 优先；否则可用 `goal_task_point_id` 或 `goal_parking_point_id`。
   - 证据：`planning_node.cpp:389`、`planning_node.cpp:391`。
2. 如果起终点无法解析，返回失败并发布 failure stop trajectory。
3. 调用 `compute_route`。
   - 证据：`planning_node.cpp:402`。
4. 将 `PlanResult` 填成 `GlobalRoute` response，并发布 `/planning/global_route`。
5. 若全局 route 失败，发布 failure status 和 failure trajectory。
6. 调用 `compute_trajectory` 生成轨迹。
   - 证据：`planning_node.cpp:412`。
7. 若轨迹为空，发布 failure stop trajectory。
8. 发布 `/planning/trajectory`。
9. response `success=true`、`message=ok`。

## 11. 消息填充

### GlobalRoute

`to_msg(const PlanResult&)` 填充：

- `route_id`
- `source_package_id`
- `planner_algorithm`
- `node_ids`
- `edge_ids`
- `length_m`
- `estimated_time_s`
- `status`

证据：`src/low_speed_av_planning/src/planning_node.cpp:314`。

### Trajectory

`to_msg(const Trajectory&)` 将内部 waypoint 映射为 `TrajectoryPoint`：

- `x_m/y_m/yaw_rad`
- `kappa_1pm`
- `s_m = route_s_m`
- `v_mps = target_speed_mps`
- `gear`
- `behavior`

证据：`src/low_speed_av_planning/src/planning_node.cpp:331`。

## 12. 代码路径表

| 文件 | 类/函数 | 职责 | 输入 | 输出 |
|---|---|---|---|---|
| `planning_node.cpp` | `PlanningNode` | ROS2 节点、参数、服务、发布 | YAML 参数、服务请求 | route/trajectory/status |
| `planning_node.cpp` | `on_reload_roadnet` | 重载 AD Package | `package_path` | response + roadnet status |
| `planning_node.cpp` | `on_plan_route` | 触发规划主流程 | 起终点 node/task/parking | `GlobalRoute` + `Trajectory` |
| `roadnet_loader.cpp` | `RoadnetLoader::load` | 读取和验证 AD Package | package 根目录 | `RoadnetPackage` |
| `topology_graph.cpp` | `TopologyGraph` | 构建拓扑邻接表 | nodes/edges | graph query |
| `dijkstra_planner.cpp` | `DijkstraPlanner::plan` | Dijkstra 搜索 | graph/start/goal/options | `PlanResult` |
| `astar_planner.cpp` | `AstarPlanner::plan` | A* 搜索 | graph/start/goal/options | `PlanResult` |
| `reference_line_motion_planner.cpp` | `make_trajectory` | waypoint 拼接 | edge_ids/package/options | 内部 trajectory |
| `curvature_speed_planner.cpp` | `apply` | 曲率限速 | trajectory/options | 更新 speed |
| `obstacle_aware_speed_planner.cpp` | `apply` | 障碍停车/降速 | trajectory/options | 更新 speed |
