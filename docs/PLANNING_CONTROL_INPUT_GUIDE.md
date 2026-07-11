# 规划/控制模块输入指南

本文说明当前工程在启动规划和控制前必须准备哪些文件、参数、topic 和服务输入。内容基于当前源码、`.msg`、`.srv`、YAML 和 launch 文件。

## 1. 总体输入关系

```text
AD Package 目录
  -> low_speed_av_planning/RoadnetLoader
  -> PlanRoute 服务触发规划
  -> /planning/global_route + /planning/trajectory
  -> low_speed_av_control 订阅定位、轨迹、车辆状态、安全状态
  -> /yunle_chassis/control/scu_control_command
```

规划模块由服务触发，不是自动从 topic 触发路线规划。控制模块周期运行，只要有有效定位和有效轨迹，就会持续计算控制输出。

## 2. 规划模块必须准备的输入

### 2.1 AD Package 路径

参数：`roadnet.package_path`

证据：

- `src/low_speed_av_planning/src/planning_node.cpp:45` 声明 `roadnet.package_path`。
- `src/low_speed_av_planning/src/planning_node.cpp:106` 在启动时从参数加载 package。
- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:31` 声明 `roadnet_package_path` launch 参数。
- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:39` 将 launch 参数注入 `roadnet.package_path`。

默认 bringup 会指向安装后的 `low_speed_av_bringup/sample_ad_package`。如果单独启动 planning 包，`roadnet.package_path` 默认为空，节点等待 `ReloadRoadnet` 服务加载。

### 2.2 AD Package canonical 文件

RoadnetLoader 当前使用 Low Speed Roadnet AD Package v1.1 canonical 路径。必须准备：

| 文件 | 用途 |
|---|---|
| `project_manifest.json` | package schema、版本、文件表、坐标系、单位、manifest hashes |
| `checksums.sha256` | 文件 SHA-256 校验 |
| `map/map_metadata.yaml` | 地图元信息，当前主要作为 package 内容保留 |
| `roadnet/topology.json` | 拓扑节点、边、cost、方向、可用性 |
| `roadnet/roadnet.json` | roadnet 原始结构，当前由 package 合同保留 |
| `roadnet/route_graph.yaml` | route graph 结构，当前由 package 合同保留 |
| `trajectory/waypoints.yaml` | 参考轨迹点，含 `x/y/yaw/kappa/v_mps/s_m/edge_id/path_id` |
| `trajectory/waypoint_index.json` | edge 到 waypoint 半开区间索引 |
| `semantics/areas.json` | speed-zone、no-go、keepout 等区域 |
| `semantics/task_points.json` | 任务点，可解析到 node 或 edge |
| `semantics/parking_points.json` | 停车点，可解析到 node 或 edge |
| `semantics/charging_points.json` | 充电点，当前加载为语义点 |
| `validation/validation_report.json` | validation status 与 blocking_errors |

证据：

- `src/low_speed_av_planning/src/roadnet_loader.cpp:213` 要求 `project_manifest.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:259` 读取 `validation/validation_report.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:268` 读取 `roadnet/topology.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:269` 读取 `trajectory/waypoints.yaml`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:270` 读取 `trajectory/waypoint_index.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:390` 读取 `semantics/areas.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:419`、`src/low_speed_av_planning/src/roadnet_loader.cpp:422`、`src/low_speed_av_planning/src/roadnet_loader.cpp:425` 读取 task/parking/charging points。

不要把旧路径 `manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json` 作为主路径。

### 2.3 Loader 校验相关参数

| 参数 | 默认 | 说明 |
|---|---:|---|
| `roadnet.reject_failed_validation` | `true` | manifest 或 validation report 失败时拒绝加载 |
| `roadnet.verify_checksums` | `true` | 校验 `checksums.sha256` 与 `manifest.hashes` |

证据：

- `src/low_speed_av_planning/src/planning_node.cpp:46`、`src/low_speed_av_planning/src/planning_node.cpp:47`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:439` 调用 checksum 验证。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:452` 实现 `verify_checksums`。

### 2.4 规划配置输入

| 参数 | 默认/bringup 值 | 作用 |
|---|---|---|
| `topics.localization_pose_topic` | `/localization/pose` | 规划参数保留，目前 planning node 未订阅定位 |
| `topics.global_route_topic` | `/planning/global_route` | 发布全局路线 |
| `topics.trajectory_topic` | `/planning/trajectory` | 发布控制参考轨迹 |
| `topics.planning_status_topic` | `/planning/status` | 发布规划模块状态 |
| `topics.roadnet_status_topic` | `/planning/roadnet_status` | 发布 roadnet 加载状态 |
| `global_planner.algorithm` | `astar` | `dijkstra` 或 `astar` |
| `global_planner.heuristic_weight` | `1.0` | A* 启发式权重 |
| `global_planner.allow_reverse` | `true` | 是否允许 reverse edge |
| `global_planner.blocked_edges` | `[]` | 运行时阻断 edge 列表 |
| `motion_planner.algorithm` | `reference_line` | `reference_line`、`stop_and_wait`、`frenet_lite`、`hybrid_astar_parking` |
| `motion_planner.horizon_distance_m` | `15.0` | 输出轨迹前视距离 |
| `motion_planner.deduplicate_edge_boundary_points` | `true` | 拼接时去重边界点 |
| `motion_planner.regenerate_route_s` | `true` | 拼接后重算 route `s_m` |
| `speed_planner.algorithm` | `curvature` | `constant`、`curvature`、`obstacle_aware` |
| `speed_planner.default_speed_mps` | `0.5` | 默认低速 |
| `speed_planner.max_speed_mps` | `1.0` | 速度上限 |
| `speed_planner.max_lateral_accel_mps2` | `0.5` | 曲率速度限制 |
| `speed_planner.obstacle_distance_m` | `-1.0` | 障碍距离输入，负值表示无障碍 |
| `speed_planner.obstacle_stop_distance_m` | `2.0` | 障碍停车距离 |

证据：

- `src/low_speed_av_planning/src/planning_node.cpp:48` 至 `src/low_speed_av_planning/src/planning_node.cpp:66`。
- `src/low_speed_av_bringup/config/planning_params.yaml:16` 至 `src/low_speed_av_bringup/config/planning_params.yaml:43`。

### 2.5 规划服务输入

实际服务字段来自 `src/low_speed_av_interfaces/srv/*.srv`。

`ReloadRoadnet.srv`：

```text
string package_path
---
bool success
string package_id
string message
```

`PlanRoute.srv`：

```text
string start_node_id
string goal_node_id
string start_task_point_id
string goal_task_point_id
string goal_parking_point_id
---
bool success
string message
GlobalRoute route
```

`SetPlannerAlgorithm.srv`：

```text
string global_planner_algorithm
string motion_planner_algorithm
string speed_planner_algorithm
---
bool success
string message
```

## 3. 控制模块必须准备的输入

### 3.1 控制订阅 topic

| 输入 | 默认 topic | 类型 | 是否必须 | 说明 |
|---|---|---|---|---|
| 定位 | `/localization/pose` | `geometry_msgs/msg/PoseStamped` | 必须 | 无定位或超时会停车 |
| 轨迹 | `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 必须 | 通常由规划模块发布 |
| 车辆状态 | `/vehicle/state` | `low_speed_av_interfaces/msg/VehicleState` | 建议 | 缺省内部状态保守为低速/默认 gear |
| 安全状态 | `/safety/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 建议 | Estop 优先级最高 |

证据：

- `src/low_speed_av_control/src/control_node.cpp:83` 订阅定位。
- `src/low_speed_av_control/src/control_node.cpp:86` 订阅轨迹。
- `src/low_speed_av_control/src/control_node.cpp:89` 订阅车辆状态。
- `src/low_speed_av_control/src/control_node.cpp:92` 订阅安全状态。

### 3.2 控制配置输入

| 参数组 | 关键参数 |
|---|---|
| `output` | 默认 `mode: "both"`，可选 `internal`、`scu_control_command`、`both` |
| `topics` | localization、trajectory、vehicle_state、safety、internal command、SCU command、status |
| `controller` | `algorithm`、`control_rate_hz`、定位/轨迹超时、trajectory status 白名单与 s 容差 |
| `vehicle_state` | `required`、VehicleState receive-time timeout |
| `safety` | `estop_latched`、`clear_speed_threshold_mps`；锁存急停通过 Trigger service 清除 |
| `vehicle` | `model`、`wheel_base_m`、speed/accel/steer 限制、`rear_steer_ratio` |
| `pure_pursuit` | lookahead 参数 |
| `stanley` | `k`、`epsilon_mps`、最大修正角 |
| `lqr` | Q/R 权重、迭代次数、收敛阈值、最低速度、preview、曲率前馈、最大转角 |
| `mpc_sampler` | horizon、dt、曲率样本、cost weights |
| `command_smoother` | 最大速度步长、最大转角速率 |
| `scu` | SCU 转角/速度范围、方向符号、停车挡位、灯光与 valid flags |

证据：

- `src/low_speed_av_control/src/control_node.cpp:17` 至 `src/low_speed_av_control/src/control_node.cpp:79`。
- `src/low_speed_av_bringup/config/control_params.yaml:4` 至文件末尾。

### 3.3 控制器与车辆模型输入

控制器算法：

- `pure_pursuit`
- `stanley`
- `lqr`
- `mpc_sampler`

车辆模型：

- `front_ackermann`
- `dual_ackermann`

证据：

- `src/low_speed_av_control/src/controller_factory.cpp:12` 至 `src/low_speed_av_control/src/controller_factory.cpp:23`。
- `src/low_speed_av_control/src/vehicle_model_factory.cpp:10` 至 `src/low_speed_av_control/src/vehicle_model_factory.cpp:15`。

## 4. 最终控制输出

最终底盘输出：

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

单位与语义：

- `scu_target_speed`：km/h，非负，由内部 `speed_mps` 取绝对值后乘 3.6。
- `scu_steering_angle_front`、`scu_steering_angle_rear`：deg，物理前/后轮转角。
- `scu_shift_level_request`：`1=D`、`2=N`、`3=R`。
- 安全停车：speed 0、前/后转角 0、brake true、shift 合法。

证据：

- `src/low_speed_av_control/src/control_node.cpp:97` 创建 SCU publisher。
- `src/low_speed_av_control/src/control_node.cpp:370` 调用 SCU mapper。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:50` m/s 到 km/h。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:35` rad 到 deg。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:63` gear 到 shift。
- `src/low_speed_av_control/src/scu_command_mapper.cpp:89` 安全停车命令。

## 5. 启动前人工确认

1. AD Package 路径正确，且包含 canonical 文件。
2. `validation.status` 通过，`blocking_errors == 0`。
3. checksum 未失配。
4. `/localization/pose` 有有效 `PoseStamped`。
5. 调用 `PlanRoute` 后 `/planning/trajectory` 有有效点。
6. `/safety/status` 未处于 estop/failure。
7. SCU 输出 topic 存在，类型为 `chassis_interfaces/msg/ScuControlCommand`。
8. 实车或台架前先验证 brake stop、D/R gear、steering sign。
