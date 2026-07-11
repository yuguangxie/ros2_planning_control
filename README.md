# ROS2 低速无人车规划、控制与仿真系统

本项目是一套面向低速无人车的 ROS2 功能包集合，用于读取路网编辑器导出的 Low Speed Roadnet AD Package，完成语义目标规划、局部轨迹发布、轨迹跟踪控制、Yunle SCU 底盘命令输出，以及 RViz 可视化和仿真定位跟随。

项目核心数据链路如下：

```text
Low Speed Roadnet AD Package
  -> low_speed_av_planning / RoadnetLoader
  -> directed topology graph + waypoints + semantics
  -> /low_speed_av_planning/plan_route 或 /low_speed_av_planning/plan_mission
  -> /planning/global_route
  -> /planning/full_reference_path
  -> /planning/trajectory
  -> low_speed_av_control
  -> /control/command
  -> /yunle_chassis/control/scu_control_command
  -> yunle_chassis/chassis_driver
  -> Yunle SCU CAN command
```

仿真链路如下：

```text
low_speed_av_simulation
  -> 初始 /localization/pose
  -> 跟随 /planning/full_reference_path 或 /planning/trajectory
  -> 实时更新 /localization/pose
  -> planning/control 形成闭环仿真流程
```

## 1. 功能包说明

### 1.1 `low_speed_av_interfaces`

接口包，只定义本项目内部规划、控制和状态消息，不包含运行节点。

主要消息：

| 接口 | 用途 |
|---|---|
| `TrajectoryPoint.msg` | 单个轨迹点，包含坐标、航向、曲率、速度、弧长、挡位和行为标签。 |
| `Trajectory.msg` | 控制模块跟踪的轨迹或完整参考路径。 |
| `GlobalRoute.msg` | 拓扑级全局路线，包含 node/edge 序列和路线长度。 |
| `ControlCommand.msg` | 控制模块内部调试命令，使用 SI 单位。 |
| `VehicleState.msg` | 车辆状态输入。 |
| `ModuleStatus.msg` | 模块状态、告警和安全状态。 |
| `RoadnetStatus.msg` | 路网加载状态。 |

主要服务：

| 服务 | 用途 |
|---|---|
| `ReloadRoadnet.srv` | 重新加载路网包。 |
| `PlanRoute.srv` | 兼容式路线规划接口，支持 node/task/parking 目标字段。 |
| `PlanMission.srv` | 推荐的任务级规划接口，支持 `current_pose`、`node`、`task`、`parking`、`charging`。 |
| `SetPlannerAlgorithm.srv` | 切换全局、运动、速度规划算法。 |
| `SetControllerAlgorithm.srv` | 切换控制算法和车辆模型。 |

### 1.2 `low_speed_av_planning`

规划包，负责读取 AD Package、构建有向拓扑图、解析语义目标点，并发布完整参考路径和控制用局部轨迹。

主要能力：

- 读取 Low Speed Roadnet AD Package v1.1。
- 校验 `project_manifest.json`、`checksums.sha256`、`validation/validation_report.json`。
- 加载 `roadnet/topology.json`、`trajectory/waypoints.yaml`、`trajectory/waypoint_index.json`。
- 加载 `semantics/task_points.json`、`semantics/parking_points.json`、`semantics/charging_points.json`、`semantics/areas.json`。
- 支持 Dijkstra 和 A* 全局规划。
- 支持 `reference_line` 运动轨迹生成。
- 支持 `constant`、`curvature`、`obstacle_aware` 速度规划。
- 支持当前 `/localization/pose` 作为起点。
- 支持 task、parking、charging 语义目标点投影到 edge waypoint。
- 支持 full reference path 与控制用 local trajectory 分离。
- 支持配置化倒车策略。
- 周期重发 `/planning/trajectory`，供控制模块持续跟踪。

### 1.3 `low_speed_av_control`

控制包，负责订阅定位和规划轨迹，执行轨迹跟踪控制，转换为车辆模型 steering command，并最终输出 Yunle SCU 底盘控制消息。

主要能力：

- 订阅 `/localization/pose`。
- 订阅 `/planning/trajectory`。
- 订阅 `/vehicle/state`。
- 订阅 `/safety/status`。
- 支持控制算法：
  - `pure_pursuit`
  - `stanley`
  - `lqr`
  - `mpc_sampler`
- 支持车辆模型：
  - `front_ackermann`
  - `dual_ackermann`
- 控制输出经过限幅、平滑和有限值检查。
- 完整消费 `Trajectory.status`、`emergency_stop`、消息标识和点合法性；规划失败轨迹不能进入控制器。
- 支持定位超时、轨迹超时、车辆状态超时、自治未许可、人工制动、车辆故障和安全急停停车。
- 通过显式 `/low_speed_av_control/clear_estop` 服务清除锁存急停，普通 OK 心跳不会自动清除。
- 默认发布内部安全合同命令 `/control/command`。
- 发布底盘命令 `/yunle_chassis/control/scu_control_command`。
- 周期发布 `/control/status`。

### 1.4 `low_speed_av_simulation`

仿真与可视化包，提供 RViz Marker 可视化和仿真定位发布。

主要节点：

| 节点 | 功能 |
|---|---|
| `roadnet_visualization_node` | 加载路网包，发布路网、语义点、车辆、规划路线和轨迹的 RViz marker。 |
| `sim_localization_pose_publisher_node` | 发布仿真 `/localization/pose`，支持固定点、路径跟随和轨迹回放。 |

主要能力：

- 发布 `/simulation/roadnet_markers`。
- 发布 `/simulation/route_markers`。
- 发布 `/simulation/trajectory_path`。
- 发布 `/simulation/vehicle_markers`。
- 发布 `/simulation/status`。
- 发布 `/simulation/pose_path`。
- 启动后先发布配置中的初始 pose。
- 规划成功后优先跟随 `/planning/full_reference_path`。
- 若 full reference path 不可用，可 fallback 到 `/planning/trajectory`。
- 对重复重发的同一条路径不重置仿真进度。
- 支持 `/simulation/start`、`/simulation/pause`、`/simulation/reset`、`/simulation/rewind_path`。

### 1.5 `low_speed_av_bringup`

启动和默认配置包。

主要内容：

| 文件 | 用途 |
|---|---|
| `launch/planning_control_demo.launch.py` | 同时启动 planning node 和 control node。 |
| `config/planning_params.yaml` | 规划默认参数。 |
| `config/control_params.yaml` | 控制默认参数。 |
| `sample_ad_package/` | 最小示例路网包。 |

### 1.6 `yunle_chassis/chassis_interfaces`

Yunle 底盘接口包，定义底盘驱动使用的 ROS2 消息。

关键消息：

```text
chassis_interfaces/msg/ScuControlCommand
```

该消息是当前控制模块最终输出到底盘驱动的控制协议。

### 1.7 `yunle_chassis/chassis_driver`

Yunle 底盘驱动包，订阅 SCU 控制命令，将 ROS2 消息转换为底盘 CAN 控制报文。

默认订阅：

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

默认 SCU 转角限制：

```yaml
scu_control_max_steering_angle_deg: 27.0
```

## 2. 路网 AD Package 数据格式

规划模块使用 Low Speed Roadnet AD Package v1.1。当前项目使用以下路径作为主协议，不使用旧版 `manifest.json`、`trajectory/waypoints.json` 或根目录 `validation_report.json` 作为主入口。

必需或常用文件：

```text
project_manifest.json
checksums.sha256
map/map_metadata.yaml
roadnet/topology.json
roadnet/roadnet.json
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
```

### 2.1 Manifest 和校验规则

`RoadnetLoader` 首先读取：

```text
project_manifest.json
```

关键要求：

- `schema` 应为 `low_speed_roadnet_ad_package`。
- `schema_version` 支持 `1.1.0` 和兼容的 `1.1.x`。
- 若 `validation.status == "failed"`，拒绝加载。
- 若 `validation.blocking_errors > 0`，拒绝加载。
- 若启用 checksum 校验，`checksums.sha256` 或 manifest hashes 不匹配时拒绝加载。

### 2.2 Waypoint 字段映射

`trajectory/waypoints.yaml` 中的 waypoint 字段映射到内部轨迹字段：

| AD Package 字段 | 内部字段 | 含义 |
|---|---|---|
| `x` | `x_m` | map 坐标系 x，单位 m。 |
| `y` | `y_m` | map 坐标系 y，单位 m。 |
| `yaw` | `yaw_rad` | 航向角，单位 rad。 |
| `kappa` | `kappa_1pm` | 曲率，单位 1/m。 |
| `v_mps` | `target_speed_mps` / `v_mps` | 目标速度，单位 m/s。 |
| `s_m` | `edge_s_m` / route `s_m` | edge 内弧长，拼接后重新生成路线弧长。 |
| `edge_id` | `edge_id` | 所属拓扑边。 |
| `path_id` | `path_id` | 所属参考线。 |
| `direction` | `direction` / gear hint | 方向或挡位提示。 |

`trajectory/waypoint_index.json` 支持：

- `end_index_exclusive`：优先使用，表示右开区间。
- `end_index`：兼容旧字段，若无 `end_index_exclusive`，按 inclusive 处理。

### 2.3 语义点

规划模块支持以下语义目标：

| 文件 | 目标类型 | PlanMission `goal_type` |
|---|---|---|
| `semantics/task_points.json` | 任务点 | `task` |
| `semantics/parking_points.json` | 停车点 | `parking` |
| `semantics/charging_points.json` | 充电点 | `charging`、`charging_point`、`charge` |

语义点解析规则：

- 优先使用有效的 `linked_node_id`。
- 若 `linked_node_id` 缺失、为空、`null` 或无效，则使用 `linked_edge_id` fallback。
- 对目标点，优先投影到 linked edge 的 waypoint，并保留目标几何位置。
- 不把 `"null"` 当作真实 node id。
- task、parking、charging 使用统一的 anchor 解析逻辑。

当前仓库包含两个正式路网包：

```text
roadnet_ad_package_20260610T012525Z_1
roadnet_ad_package_20260610T012525Z_2
```

这些路网包作为输入数据使用，不应在运行调试中直接修改。

## 3. 主要话题

### 3.1 Planning 话题

| Topic | Type | 含义 |
|---|---|---|
| `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | 拓扑级路线，包含 node/edge 序列，主要用于调试、可视化和上位机理解路线。 |
| `/planning/full_reference_path` | `low_speed_av_interfaces/msg/Trajectory` | 完整连续几何参考路径，包含从起点到目标点的连续 waypoint 序列。 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制模块实际跟踪的局部轨迹，默认约 10 Hz 重发。 |
| `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 规划状态、成功、失败和诊断信息。 |
| `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | 路网加载状态，使用 transient local QoS 并周期发布。 |

`/planning/global_route` 是拓扑路线，不直接给控制模块跟踪。控制模块跟踪的是 `/planning/trajectory`。

### 3.2 Control 话题

| Topic | Type | 含义 |
|---|---|---|
| `/localization/pose` | `geometry_msgs/msg/PoseStamped` | 控制和规划使用的当前定位。 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制模块输入轨迹。 |
| `/vehicle/state` | `low_speed_av_interfaces/msg/VehicleState` | 车辆状态输入。 |
| `/safety/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 安全状态输入。 |
| `/control/command` | `low_speed_av_interfaces/msg/ControlCommand` | 内部调试控制命令，SI 单位。 |
| `/control/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 控制状态心跳和安全停车原因。 |
| `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | 最终下发给 Yunle chassis driver 的 SCU 控制命令。 |

### 3.3 Simulation 话题

| Topic | Type | 含义 |
|---|---|---|
| `/simulation/roadnet_markers` | `visualization_msgs/msg/MarkerArray` | 路网节点、边、waypoint、语义点可视化。 |
| `/simulation/route_markers` | `visualization_msgs/msg/MarkerArray` | 全局路线和目标状态可视化。 |
| `/simulation/trajectory_path` | `nav_msgs/msg/Path` | 当前控制轨迹或 full reference path 的 RViz path。 |
| `/simulation/vehicle_markers` | `visualization_msgs/msg/MarkerArray` | 当前车辆位置和朝向 marker。 |
| `/simulation/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 仿真状态。 |
| `/simulation/pose_path` | `nav_msgs/msg/Path` | 仿真定位走过的路径。 |

### 3.4 Chassis 话题

| Topic | Type | 含义 |
|---|---|---|
| `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | 底盘 SCU 控制命令输入。 |

## 4. 服务接口

### 4.1 重新加载路网

```text
/low_speed_av_planning/reload_roadnet
low_speed_av_interfaces/srv/ReloadRoadnet
```

请求字段：

| 字段 | 含义 |
|---|---|
| `package_path` | AD Package 目录路径。为空时使用参数 `roadnet.package_path`。 |

### 4.2 推荐任务规划接口

```text
/low_speed_av_planning/plan_mission
low_speed_av_interfaces/srv/PlanMission
```

请求字段：

| 字段 | 含义 |
|---|---|
| `start_type` | 起点类型。支持 `""`、`current_pose`、`node`、`task`、`parking`、`charging`。 |
| `start_id` | 起点 ID。`current_pose` 可为空。 |
| `goal_type` | 目标类型。支持 `node`、`task`、`parking`、`charging`、`charging_point`、`charge`。 |
| `goal_id` | 目标 ID。 |

推荐使用 `PlanMission` 作为上层业务接口。例如当前定位到任务点：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"
```

### 4.3 兼容路线规划接口

```text
/low_speed_av_planning/plan_route
low_speed_av_interfaces/srv/PlanRoute
```

请求字段：

| 字段 | 含义 |
|---|---|
| `start_node_id` | 起点 topology node id。为空时可使用当前定位作为起点。 |
| `goal_node_id` | 目标 topology node id。 |
| `start_task_point_id` | 起点 task point id。 |
| `goal_task_point_id` | 目标 task point id。 |
| `goal_parking_point_id` | 目标 parking point id。 |

`PlanRoute` 保留用于兼容、调试和拓扑回归测试。业务目标建议使用 `PlanMission`。

### 4.4 算法切换

规划算法切换：

```text
/low_speed_av_planning/set_planner_algorithm
low_speed_av_interfaces/srv/SetPlannerAlgorithm
```

控制算法切换：

```text
/low_speed_av_control/set_controller_algorithm
low_speed_av_interfaces/srv/SetControllerAlgorithm
```

### 4.5 仿真控制服务

| Service | Type | 含义 |
|---|---|---|
| `/simulation/start` | `std_srvs/srv/Trigger` | 开始或继续路径跟随。 |
| `/simulation/pause` | `std_srvs/srv/Trigger` | 暂停移动，但继续发布当前 pose。 |
| `/simulation/reset` | `std_srvs/srv/Trigger` | 回到配置初始 pose。 |
| `/simulation/rewind_path` | `std_srvs/srv/Trigger` | 路径进度回到路径起点。 |

## 5. Yunle SCU 控制协议

控制模块最终发布：

```text
Topic: /yunle_chassis/control/scu_control_command
Type:  chassis_interfaces/msg/ScuControlCommand
```

`ScuControlCommand` 字段：

| 字段 | 单位/合法值 | 说明 |
|---|---|---|
| `scu_shift_level_request` | `1=D`、`2=N`、`3=R` | 挡位请求。不得发布其他值。 |
| `scu_steering_angle_front` | deg | 前轮物理转角。 |
| `scu_steering_angle_rear` | deg | 后轮物理转角。 |
| `scu_target_speed` | km/h | 目标速度，必须非负。方向由挡位决定。 |
| `scu_brake_enable` | bool | 制动使能。安全停车时为 true。 |
| `gw_left_turn_light_request` | uint8 | 左转灯请求。默认 0。 |
| `gw_right_turn_light_request` | uint8 | 右转灯请求。默认 0。 |
| `gw_position_light_request` | uint8 | 位置灯请求。默认 0。 |
| `gw_low_beam_request` | uint8 | 近光灯请求。默认 0。 |
| `scu_torque_or_speed_mode` | uint8 | 默认 1。 |
| `steering_angle_speed_valid` | bool | 默认 false。 |
| `brake_force_command_valid` | bool | 默认 false。 |

单位转换规则：

| 内部命令 | SCU 输出 |
|---|---|
| `target_speed_mps` | `abs(target_speed_mps) * 3.6` -> `scu_target_speed`，单位 km/h。 |
| `front_steering_angle_rad` | rad -> deg -> `scu_steering_angle_front`。 |
| `rear_steering_angle_rad` | rad -> deg -> `scu_steering_angle_rear`。 |
| `gear=drive` | `scu_shift_level_request=1`。 |
| `gear=reverse` | `scu_shift_level_request=3`，速度仍为非负 km/h。 |
| 安全停车 | brake=true，speed=0，front/rear steering=0，shift 使用有效 stop shift。 |

默认 SCU 输出限制：

```yaml
scu:
  max_steering_angle_deg: 27.0
  max_target_speed_kmh: 5.0
  overrange_policy: "clamp"
```

底盘驱动侧默认 steering 限制也是 27 deg。运行前应确认 control 和 chassis driver 配置一致。

## 6. 配置文件

### 6.1 规划配置

默认配置：

```text
src/low_speed_av_bringup/config/planning_params.yaml
src/low_speed_av_planning/config/planning_params.yaml
```

关键参数：

```yaml
roadnet:
  package_path: ""
  reject_failed_validation: true
  verify_checksums: true

topics:
  localization_pose_topic: "/localization/pose"
  global_route_topic: "/planning/global_route"
  trajectory_topic: "/planning/trajectory"
  planning_status_topic: "/planning/status"
  roadnet_status_topic: "/planning/roadnet_status"

global_planner:
  algorithm: "astar"        # astar | dijkstra
  allow_reverse: true

motion_planner:
  algorithm: "reference_line"
  horizon_distance_m: 15.0

speed_planner:
  algorithm: "curvature"    # constant | curvature | obstacle_aware
  default_speed_mps: 0.5
  max_speed_mps: 1.0

planning:
  use_current_pose_as_start: true
  localization_timeout_s: 1.0
  republish_last_trajectory: true
  trajectory_republish_rate_hz: 10.0
  publish_full_reference_path: true
  full_reference_path_topic: "/planning/full_reference_path"
  local_trajectory_from_current_pose: true
  arrival_radius_m: 0.5
  arrival_heading_tolerance_rad: 0.35
  reverse:
    allow_reverse_planning: false
    allow_reverse_local_segment: false
    prefer_forward_route_when_reverse_disabled: true
  start_anchor:
    include_current_edge_prefix: true
    max_first_trajectory_point_distance_m: 2.0
```

倒车策略说明：

- 默认不静默允许倒车。
- 若目标位于当前 edge 后方，且倒车关闭，规划会优先尝试有向拓扑前向绕行。
- 若倒车开启且允许 local reverse segment，规划结果和状态信息会明确说明 reverse segment 被选择。

### 6.2 控制配置

默认配置：

```text
src/low_speed_av_bringup/config/control_params.yaml
src/low_speed_av_control/config/control_params.yaml
```

关键参数：

```yaml
output:
  mode: "both"  # internal | scu_control_command | both

topics:
  localization_pose_topic: "/localization/pose"
  trajectory_topic: "/planning/trajectory"
  vehicle_state_topic: "/vehicle/state"
  safety_status_topic: "/safety/status"
  control_command_topic: "/control/command"
  scu_command_topic: "/yunle_chassis/control/scu_control_command"
  control_status_topic: "/control/status"

controller:
  algorithm: "lqr"             # pure_pursuit | stanley | lqr | mpc_sampler
  control_rate_hz: 50.0
  localization_timeout_s: 0.2
  trajectory_timeout_s: 0.5
  allowed_trajectory_statuses: ["ok"]
  trajectory_s_tolerance_m: 1.0e-4

vehicle_state:
  required: false
  timeout_s: 0.5

vehicle:
  model: "front_ackermann"     # front_ackermann | dual_ackermann
  wheel_base_m: 1.2
  rear_steer_ratio: 0.5

control:
  status_publish_rate_hz: 5.0

safety:
  estop_latched: true
  clear_speed_threshold_mps: 0.05

scu:
  max_steering_angle_deg: 27.0
  max_target_speed_kmh: 5.0
  overrange_policy: "clamp"
  stop_shift_level: 1
```

### 6.3 仿真配置

默认配置：

```text
src/low_speed_av_simulation/config/simulation_params.yaml
```

关键参数：

```yaml
sim_localization_pose_publisher:
  ros__parameters:
    simulation:
      localization:
        pose_topic: "/localization/pose"
        publish_rate_hz: 20.0
        frame_id: "map"
        mode: "path_follow"       # fixed_pose | path_follow | trajectory_replay | roadnet_waypoint_replay
        initial_pose:
          source: "explicit"      # explicit | waypoint | task_point | edge_progress
          x: 0.554
          y: 1.473
          yaw: -0.9178
        follow:
          follow_source: "full_reference_path"
          fallback_to_local_trajectory: true
          reanchor_on_new_path: true
          restart_on_new_path: false
          ignore_republished_same_path: true
```

### 6.4 底盘驱动配置

默认配置：

```text
src/yunle_chassis/chassis_driver/config/chassis_driver.yaml
```

关键参数：

```yaml
topic_prefix: /yunle_chassis
default_qos_depth: 10
scu_control_max_steering_angle_deg: 27.0
scu_control_max_target_speed_kmh: 15.0
```

底盘驱动订阅 topic 由 `topic_prefix` 和相对 topic 拼接得到，默认与控制模块输出一致：

```text
/yunle_chassis/control/scu_control_command
```

## 7. 构建流程

在 Ubuntu ROS2 环境中执行：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

如果只进行无 ROS2 的数据和脚本检查，可运行纯 Python 脚本：

```bash
python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_1
python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_2
python scripts/offline_trajectory_continuity_smoke.py
python scripts/offline_reverse_policy_smoke.py
python scripts/offline_semantic_goal_followup_smoke.py
python scripts/offline_sim_localization_follow_smoke.py
python scripts/offline_scu_lqr_smoke.py
```

## 8. 标准仿真运行流程

以下流程用于 RViz + 仿真定位 + planning/control 联合验证。

### 8.1 启动仿真与可视化

设置路网包路径：

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1
```

启动可视化和仿真定位：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  rviz:=true
```

启动后，仿真定位节点会先按配置发布初始 `/localization/pose`。

### 8.2 启动规划和控制

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET
```

确认节点、话题和服务：

```bash
ros2 node list
ros2 topic list
ros2 service list
ros2 topic echo --once /planning/roadnet_status
ros2 topic echo --once /localization/pose
```

### 8.3 下发任务点目标

推荐使用 `PlanMission`：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"
```

停车点示例：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'parking', goal_id: 'RP-015'}"
```

充电点示例：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

实际可用的语义点 ID 取决于当前加载的路网包。

### 8.4 兼容 node-id 规划

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

当前定位作为起点，目标为 node：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0008', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

### 8.5 观察规划输出

```bash
ros2 topic echo --once /planning/global_route
ros2 topic echo --once /planning/full_reference_path
ros2 topic echo --once /planning/trajectory
ros2 topic echo /planning/status
```

含义：

- `/planning/global_route`：完整拓扑路线。
- `/planning/full_reference_path`：完整连续几何路径。
- `/planning/trajectory`：控制用局部轨迹，持续发布。

### 8.6 观察仿真定位跟随

```bash
ros2 topic echo /localization/pose
ros2 topic echo /simulation/status
ros2 topic echo /simulation/pose_path
```

路径跟随时，`/localization/pose` 应沿 `/planning/full_reference_path` 连续变化。若收到 `failure_stop` 或无效路径，仿真定位应停止移动并保持当前位置。

仿真控制：

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
```

### 8.7 观察控制输出

```bash
ros2 topic echo /control/status
ros2 topic echo /control/command
ros2 topic echo /yunle_chassis/control/scu_control_command
```

正常跟踪时：

- `/control/status` 应持续发布 tracking 或相关状态。
- `/yunle_chassis/control/scu_control_command` 的速度单位为 km/h。
- steering 单位为 deg。
- 倒车通过 `scu_shift_level_request=3` 表示，速度仍为非负。

## 9. 启动 Yunle 底盘驱动

底盘驱动应只在 bench、架车、车辆禁用或安全测试区域中使用。启动命令：

```bash
ros2 launch chassis_driver chassis_driver.launch.py
```

检查 topic 类型和连接：

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 interface show chassis_interfaces/msg/ScuControlCommand
ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- control publisher count 为 1。
- chassis driver subscriber count 为 1。
- topic type 为 `chassis_interfaces/msg/ScuControlCommand`。
- 不出现非法 shift。
- speed 非负。
- steering 不超过底盘配置范围。

## 10. 典型验证项目

### 10.1 路网加载

```bash
ros2 topic echo --once /planning/roadnet_status
```

应确认：

- `ready=true`。
- package id 正确。
- validation 未失败。
- blocking errors 为 0。

### 10.2 当前定位作为起点

```bash
ros2 topic echo --once /localization/pose
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"
```

应确认：

- 响应 `success=true`。
- message 中包含 start matched 信息。
- `/planning/trajectory` 首点接近当前 pose 或投影点。

### 10.3 语义目标规划

验证 task、parking、charging：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"

ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'parking', goal_id: 'RP-015'}"

ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

若某路网包不包含 parking 或 charging 点，应返回清晰失败原因，而不是空轨迹或错误 topology node。

### 10.4 轨迹连续性

```bash
ros2 topic echo --once /planning/full_reference_path
ros2 topic echo --once /planning/trajectory
```

应确认：

- `/planning/full_reference_path` 是完整连续几何路径。
- `/planning/trajectory` 是从当前 pose 附近开始的连续局部轨迹。
- 不出现“前视段 + 远处目标 edge 直接拼接”的几何跳变。

### 10.5 倒车策略

查看参数：

```bash
ros2 param get /low_speed_av_planning planning.reverse.allow_reverse_planning
ros2 param get /low_speed_av_planning planning.reverse.allow_reverse_local_segment
```

默认配置不允许静默倒车。若启用倒车：

```bash
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_planning true
ros2 param set /low_speed_av_planning planning.reverse.allow_reverse_local_segment true
```

规划响应和 `/planning/status` 应明确说明是否选择 reverse local segment。

### 10.6 SCU 安全停车

安全停车输出应满足：

- `scu_brake_enable=true`
- `scu_target_speed=0`
- `scu_steering_angle_front=0`
- `scu_steering_angle_rear=0`
- `scu_shift_level_request` 为 1、2 或 3 中的有效值

可通过以下 topic 观察：

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic echo /control/status
```

## 11. 安全边界

1. 仿真定位 `/localization/pose` 只用于仿真和 bench 验证，不应在真实车辆运动测试中默认接入实车定位链路。
2. 真实底盘测试前必须确认 E-stop、操作员、架车或安全场地。
3. 控制模块默认低速配置不等同于实车可直接运行配置。
4. 若路网校验报告包含高曲率、曲率连续性或 waypoint 曲率超限 warning，实车前应降低速度、启用曲率限速、平滑路径或重新导出路网。
5. `/planning/trajectory` 是控制输入，必须保持连续、有限值、无异常跳变。
6. 所有安全失败路径应输出 stop/failure trajectory，并使控制模块进入安全停车。
7. SCU shift 只能为 `1=D`、`2=N`、`3=R`；控制模块不得发布非法 shift。
8. 倒车规划必须由配置显式允许，不应静默启用。

## 12. 常用排查命令

查看接口：

```bash
ros2 interface show low_speed_av_interfaces/srv/PlanMission
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show chassis_interfaces/msg/ScuControlCommand
```

查看参数：

```bash
ros2 param list /low_speed_av_planning
ros2 param list /low_speed_av_control
ros2 param get /low_speed_av_planning roadnet.package_path
ros2 param get /low_speed_av_control topics.scu_command_topic
ros2 param get /low_speed_av_control scu.max_steering_angle_deg
```

查看频率：

```bash
ros2 topic hz /planning/trajectory
ros2 topic hz /control/status
ros2 topic hz /localization/pose
ros2 topic hz /yunle_chassis/control/scu_control_command
```

查看状态：

```bash
ros2 topic echo /planning/status
ros2 topic echo /planning/roadnet_status
ros2 topic echo /simulation/status
ros2 topic echo /control/status
```

## 13. 推荐阅读顺序

更详细的设计和验证说明位于 `docs/`：

| 文档 | 内容 |
|---|---|
| `docs/PLANNING_CONTROL_INPUT_GUIDE.md` | 规划和控制模块需要的输入数据。 |
| `docs/ROUTE_PLANNING_OPERATION_GUIDE.md` | 如何触发路线规划。 |
| `docs/TECHNICAL_ROUTE_AND_DATAFLOW.md` | 从路网到 SCU 输出的数据流。 |
| `docs/SEMANTIC_GOAL_PLANNING_DESIGN.md` | 语义目标规划策略。 |
| `docs/PLANNING_OUTPUT_DATA_CONTRACT.md` | `global_route`、`full_reference_path`、`trajectory` 的数据合同。 |
| `docs/TRAJECTORY_CONTINUITY_AND_FULL_REFERENCE_PATH_DESIGN.md` | 完整参考路径和局部轨迹连续性设计。 |
| `docs/REVERSE_PLANNING_POLICY.md` | 倒车规划策略。 |
| `docs/CURRENT_POSE_START_ANCHOR_DESIGN.md` | 当前定位位于 edge 中段时的起点处理。 |
| `docs/SIM_LOCALIZATION_PATH_FOLLOW_DESIGN.md` | 仿真定位路径跟随设计。 |
| `docs/REALTIME_SIMULATION_WORKFLOW.md` | 实时仿真使用流程。 |
| `docs/YUNLE_SCU_COMMAND_OUTPUT.md` | Yunle SCU 输出字段和单位映射。 |
| `docs/LQR_CONTROLLER_DESIGN.md` | LQR 控制器设计。 |

## Phase 14 自动化测试入口

跨平台离线检查的统一入口为：

```powershell
uv run --python "C:\Program Files\FreeCAD 1.2\bin\python.exe" scripts/run_offline_checks.py
```

也可直接使用任意可用的 Python 3 执行 `scripts/run_offline_checks.py`。该入口验证 Python 数据合同、sample/正式 Roadnet 包、template/config/sample 同步和仓库卫生；它不证明生产 C++ 或 ROS2 运行行为。

在 ROS2 Humble 环境中，生产链接的 C++ gtest 与 launch test 通过 `colcon build`、`colcon test` 执行。当前 Windows 审计环境没有 ROS2/colcon/C++ 工具链，因此这些测试源码状态为 `GENERATED_NOT_EXECUTED` 或 `SKIPPED_ROS2_UNAVAILABLE`，GitHub Actions 状态为 `CONFIGURED_NOT_EXECUTED`。Chassis 独立命令 watchdog 仍是 `CDX-P0-002` 已知生产缺口，其规格测试不得计为通过。
