# 规划、定位输入与完整技术路线说明

本文集中说明当前工程中规划、定位输入、控制输出以及各模块之间的技术路线。它不是替代代码级文档，而是给现场联调、上位机接入、路网编辑器数据导入和人工验证使用的参考说明。

## 1. 当前结论

当前工程已经有分散文档说明规划和控制输入，例如 `docs/PLANNING_CONTROL_INPUT_GUIDE.md`、`docs/ROUTE_PLANNING_OPERATION_GUIDE.md`、`docs/TECHNICAL_ROUTE_AND_DATAFLOW.md`。但这些文档没有把“规划如何使用外部路网数据”“定位如何接入”“控制最后输出什么”“完整技术路线是什么”集中在一份文档里，因此新增本文作为统一入口。

需要特别注意：当前仓库没有实现独立的定位算法模块。定位由外部定位、SLAM、融合定位或仿真系统提供，控制节点只订阅定位结果。默认定位输入 topic 是：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

代码依据：

- `src/low_speed_av_control/src/control_node.cpp:17` 声明 `topics.localization_pose_topic`，默认值为 `/localization/pose`。
- `src/low_speed_av_control/src/control_node.cpp:83` 创建 `geometry_msgs::msg::PoseStamped` 订阅。
- `src/low_speed_av_bringup/config/control_params.yaml:9` bringup 默认配置同样使用 `/localization/pose`。

规划节点目前声明了 `topics.localization_pose_topic` 参数，但当前路线规划触发主要依赖 `PlanRoute` 服务和 AD Package 数据；规划节点没有订阅定位 topic。也就是说，当前版本中“定位”主要服务于控制跟踪，不是触发全局路线规划的必需输入。

代码依据：

- `src/low_speed_av_planning/src/planning_node.cpp:48` 声明 `topics.localization_pose_topic`。
- `src/low_speed_av_planning/src/planning_node.cpp:82` 创建 `ReloadRoadnet` 服务。
- `src/low_speed_av_planning/src/planning_node.cpp:89` 创建 `PlanRoute` 服务。
- `src/low_speed_av_planning/src/planning_node.cpp:96` 创建 `SetPlannerAlgorithm` 服务。

## 2. 模块角色

```text
low_speed_av_interfaces
  -> 定义规划、控制、状态、服务接口。

low_speed_av_planning
  -> 读取 Low Speed Roadnet AD Package v1.1。
  -> 校验 manifest、validation report、checksum、拓扑、waypoint index。
  -> 通过 PlanRoute 服务触发全局规划。
  -> 输出 GlobalRoute 和 Trajectory。

外部定位模块
  -> 不在当前仓库中实现。
  -> 发布 /localization/pose，类型 geometry_msgs/msg/PoseStamped。
  -> 控制节点用它计算车辆相对轨迹的横向误差和航向误差。

low_speed_av_control
  -> 订阅定位、轨迹、车辆状态、安全状态。
  -> 运行 pure_pursuit、stanley、lqr 或 mpc_sampler。
  -> 通过 front_ackermann 或 dual_ackermann 车辆模型转换为前/后轮转角。
  -> 限幅、平滑、有限值检查。
  -> 映射为 Yunle SCU 底盘命令。

low_speed_av_bringup
  -> 提供 launch、默认参数、sample AD Package。
```

## 3. 让规划模块跑起来需要准备什么

### 3.1 必须准备 AD Package v1.1 目录

规划模块读取的是解压后的 Low Speed Roadnet AD Package 目录。必须优先使用当前规范路径，不应使用旧路径作为主路径。

必需或建议存在的文件如下：

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

关键兼容要求：

- `project_manifest.json` 中 `schema` 必须是 `low_speed_roadnet_ad_package`。
- 支持 `schema_version == "1.1.0"` 以及兼容的 `1.1.x` patch 版本。
- `validation/validation_report.json` 不能是 failed，`blocking_errors` 必须为 0。
- `trajectory/waypoints.yaml` 是轨迹点主输入。
- `trajectory/waypoint_index.json` 用于从 edge id 映射到 waypoint 区间。
- 如果开启 checksum 校验，`checksums.sha256` 或 manifest hashes 与实际文件不一致时会拒绝加载。

代码依据：

- `src/low_speed_av_planning/src/planning_node.cpp:45` 声明 `roadnet.package_path`。
- `src/low_speed_av_planning/src/planning_node.cpp:46` 声明 `roadnet.reject_failed_validation`。
- `src/low_speed_av_planning/src/planning_node.cpp:47` 声明 `roadnet.verify_checksums`。
- `src/low_speed_av_planning/src/planning_node.cpp:111` 当 `roadnet.package_path` 为空时，规划节点等待 `ReloadRoadnet`。

### 3.2 必须设置 roadnet.package_path 或调用 ReloadRoadnet

方式一：通过 launch/YAML 设置：

```yaml
roadnet:
  package_path: "/absolute/path/to/sample_ad_package"
  reject_failed_validation: true
  verify_checksums: true
```

方式二：节点启动后调用服务重新加载：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/sample_ad_package'}"
```

实际服务字段依据：

- `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv:2` 请求字段是 `string package_path`。
- `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv:4` 响应字段包含 `bool success`。
- `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv:5` 响应字段包含 `string package_id`。
- `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv:6` 响应字段包含 `string message`。

### 3.3 触发规划需要调用 PlanRoute 服务

当前规划不是自动根据车辆当前位置启动，而是通过服务请求触发。典型样例：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

实际服务字段依据：

- `src/low_speed_av_interfaces/srv/PlanRoute.srv:2` `start_node_id`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:3` `goal_node_id`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:4` `start_task_point_id`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:5` `goal_task_point_id`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:6` `goal_parking_point_id`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:9` 响应 `success`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:10` 响应 `message`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv:11` 响应 `GlobalRoute route`

## 4. 规划模块最后输出什么

规划成功后主要发布：

```text
/planning/global_route
low_speed_av_interfaces/msg/GlobalRoute

/planning/trajectory
low_speed_av_interfaces/msg/Trajectory

/planning/status
low_speed_av_interfaces/msg/ModuleStatus

/planning/roadnet_status
low_speed_av_interfaces/msg/RoadnetStatus
```

代码依据：

- `src/low_speed_av_planning/src/planning_node.cpp:49` 声明 `/planning/trajectory` 参数默认值。
- `src/low_speed_av_planning/src/planning_node.cpp:50` 声明 `/planning/global_route` 参数默认值。
- `src/low_speed_av_planning/src/planning_node.cpp:51` 声明 `/planning/status` 参数默认值。
- `src/low_speed_av_planning/src/planning_node.cpp:52` 声明 `/planning/roadnet_status` 参数默认值。
- `src/low_speed_av_planning/src/planning_node.cpp:73` 创建 GlobalRoute publisher。
- `src/low_speed_av_planning/src/planning_node.cpp:75` 创建 Trajectory publisher。
- `src/low_speed_av_planning/src/planning_node.cpp:77` 创建 planning status publisher。
- `src/low_speed_av_planning/src/planning_node.cpp:79` 创建 roadnet status publisher。

规划输出的用途：

- `/planning/global_route` 用于查看全局节点和边序列。
- `/planning/trajectory` 是控制模块的主要输入。
- `/planning/status` 用于判断规划成功、失败或异常原因。
- `/planning/roadnet_status` 用于判断 AD Package 是否已经加载并可用于规划。

## 5. 定位输入如何准备

当前控制节点要求外部模块发布定位。默认输入：

```text
topic: /localization/pose
type: geometry_msgs/msg/PoseStamped
```

推荐要求：

- `header.frame_id` 与 AD Package 中的全局坐标系一致，通常是 `map`。
- 位置单位是米。
- 姿态使用四元数，控制节点会从四元数计算 yaw。
- 定位点应与控制参考点一致，例如 manifest 中的 `coordinate_system.control_reference_frame` 若为 rear axle，则外部定位最好已经转换到后轴参考点。
- 发布频率必须高于控制节点超时要求。默认 `controller.localization_timeout_s` 为 `0.2` 秒，因此建议至少 20 Hz，调试时不低于 10 Hz。

代码依据：

- `src/low_speed_av_control/src/control_node.cpp:30` 默认定位超时 `0.2` 秒。
- `src/low_speed_av_control/src/control_node.cpp:83` 订阅定位 PoseStamped。
- `src/low_speed_av_control/src/control_node.cpp:212` `on_pose` 回调接收定位。

示例定位发布命令，仅用于真实 ROS2 环境或仿真调试：

```bash
ros2 topic pub --rate 20 /localization/pose geometry_msgs/msg/PoseStamped \
  "{header: {frame_id: 'map'}, pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {x: 0.0, y: 0.0, z: 0.0, w: 1.0}}}"
```

如果定位没有发布或超时：

- 控制节点不会输出正常跟踪命令。
- 控制节点应输出安全停车命令。
- `/control/status` 中应能看到类似定位超时的状态或原因。

## 6. 控制模块需要什么输入

控制节点正常输出底盘命令前，至少需要：

```text
/localization/pose
geometry_msgs/msg/PoseStamped

/planning/trajectory
low_speed_av_interfaces/msg/Trajectory
```

可选但建议接入：

```text
/vehicle/state
low_speed_av_interfaces/msg/VehicleState

/safety/status
low_speed_av_interfaces/msg/ModuleStatus
```

代码依据：

- `src/low_speed_av_control/src/control_node.cpp:17` 定位 topic 默认值。
- `src/low_speed_av_control/src/control_node.cpp:18` 轨迹 topic 默认值。
- `src/low_speed_av_control/src/control_node.cpp:19` 车辆状态 topic 默认值。
- `src/low_speed_av_control/src/control_node.cpp:20` 安全状态 topic 默认值。
- `src/low_speed_av_control/src/control_node.cpp:86` 创建 trajectory 订阅。
- `src/low_speed_av_control/src/control_node.cpp:89` 创建 vehicle state 订阅。
- `src/low_speed_av_control/src/control_node.cpp:92` 创建 safety status 订阅。

控制节点周期运行，默认频率由 `controller.control_rate_hz` 配置。每个周期会检查：

1. 是否存在安全急停。
2. 定位是否超时。
3. 轨迹是否超时。
4. 轨迹是否为空。
5. 控制器输出是否有限。
6. 车辆模型转换是否有限。
7. 限幅和平滑后的命令是否有效。

## 7. 控制模块最后输出什么

当前面向云乐底盘的最终输出为：

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

代码依据：

- `src/low_speed_av_control/src/control_node.cpp:22` 默认 SCU topic 是 `/yunle_chassis/control/scu_control_command`。
- `src/low_speed_av_control/src/control_node.cpp:24` 默认 `output.mode` 是 `scu_control_command`。
- `src/low_speed_av_control/src/control_node.cpp:97` 创建 `chassis_interfaces::msg::ScuControlCommand` publisher。
- `src/low_speed_av_bringup/config/control_params.yaml:15` bringup 默认配置 SCU 输出 topic。

单位和协议约束：

- 内部控制算法仍使用 SI 单位：速度 m/s、转角 rad、曲率 1/m。
- 输出到 SCU 时转换为底盘协议单位：速度 km/h、前/后轮转角 deg。
- `scu_shift_level_request` 只能是 `1=D`、`2=N`、`3=R`。
- 速度必须非负，倒车通过 R 挡表达，不通过负速度表达。
- 安全停车、急停、定位超时、轨迹超时、空轨迹、无效命令都应输出制动停车命令。

如果保留内部调试输出，则内部命令 topic 为：

```text
/control/command
low_speed_av_interfaces/msg/ControlCommand
```

该 topic 主要用于调试和兼容，不是当前底盘的最终控制接口。

## 8. 完整技术路线

```text
路网编辑器导出的 AD Package ZIP/目录
  -> project_manifest.json / checksums.sha256 / validation_report 校验
  -> roadnet/topology.json 构建有向图
  -> trajectory/waypoints.yaml 读取几何参考线点
  -> trajectory/waypoint_index.json 建立 edge -> waypoint range 映射
  -> semantics/areas.json 等语义文件加载 speed_zone / no_go / keepout 等信息
  -> ReloadRoadnet 服务或 roadnet.package_path 参数使规划节点进入 ready
  -> PlanRoute 服务输入 start/goal
  -> Dijkstra 或 A* 输出 node_ids + edge_ids
  -> reference_line / stop_and_wait 等 motion planner 生成 Trajectory
  -> constant / curvature / obstacle_aware speed planner 写入目标速度
  -> /planning/global_route
  -> /planning/trajectory
  -> 控制节点订阅 /planning/trajectory 与 /localization/pose
  -> pure_pursuit / stanley / lqr / mpc_sampler 计算期望曲率
  -> front_ackermann 或 dual_ackermann 转换为前/后轮转角
  -> CommandLimiter 限幅
  -> CommandSmoother 平滑
  -> NaN/Inf guard 有限值保护
  -> ScuCommandMapper 转换为 Yunle SCU 协议字段
  -> /yunle_chassis/control/scu_control_command
  -> chassis_driver
  -> 底盘 CAN SCU_Control_Command 0x121
```

## 9. 规划、定位、控制的运行顺序

真实 ROS2 环境中的推荐顺序：

1. 准备 AD Package 目录，并确认 checksum、validation report 正常。
2. 启动底盘驱动，但不要让车辆处于可运动危险状态。
3. 启动外部定位模块，使其持续发布 `/localization/pose`。
4. 启动 planning/control bringup：

   ```bash
   ros2 launch low_speed_av_bringup planning_control_demo.launch.py
   ```

5. 确认节点、服务和 topic：

   ```bash
   ros2 node list
   ros2 service list
   ros2 topic list
   ```

6. 确认路网状态：

   ```bash
   ros2 topic echo /planning/roadnet_status
   ```

7. 如果启动时没有设置 package path，调用 `ReloadRoadnet`：

   ```bash
   ros2 service call /low_speed_av_planning/reload_roadnet \
     low_speed_av_interfaces/srv/ReloadRoadnet \
     "{package_path: '/absolute/path/to/sample_ad_package'}"
   ```

8. 调用 `PlanRoute`：

   ```bash
   ros2 service call /low_speed_av_planning/plan_route \
     low_speed_av_interfaces/srv/PlanRoute \
     "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
   ```

9. 查看规划输出：

   ```bash
   ros2 topic echo /planning/global_route
   ros2 topic echo /planning/trajectory
   ros2 topic echo /planning/status
   ```

10. 查看控制和底盘输出：

    ```bash
    ros2 topic echo /control/status
    ros2 topic echo /yunle_chassis/control/scu_control_command
    ```

## 10. 常见失败现象和定位方法

| 现象 | 可能原因 | 检查方法 | 期望处理 |
|---|---|---|---|
| planning 节点提示等待路网 | `roadnet.package_path` 为空 | `ros2 param get /low_speed_av_planning roadnet.package_path` | 设置参数或调用 `ReloadRoadnet` |
| roadnet 不 ready | validation failed、blocking_errors > 0、checksum mismatch、文件缺失 | `ros2 topic echo /planning/roadnet_status` | 修复 AD Package 后重新加载 |
| PlanRoute 失败 | 起终点 node 不存在、边被 no-go/keepout 阻断、拓扑不连通 | 查看服务响应 `message` 和 `/planning/status` | 更换起终点或修复路网 |
| `/planning/trajectory` 没有输出 | 路网未加载、规划失败、motion planner 没有生成轨迹 | echo `/planning/status` | 先修复规划状态 |
| 控制不输出正常速度 | 没有定位、没有轨迹、超时、estop | echo `/control/status` | 持续发布定位和轨迹，检查安全状态 |
| SCU 输出 brake true、speed 0 | 安全停车路径被触发 | echo `/control/status` 和 `/safety/status` | 按状态原因解除 |
| 底盘不接受命令 | shift 非法、速度/转角越界、驱动未启动 | echo SCU topic，检查 chassis_driver 日志 | 确认字段合法、驱动在线 |

## 11. 数据准备清单

| 类别 | 必需性 | 内容 | 当前默认 |
|---|---:|---|---|
| AD Package | 必需 | v1.1 canonical 目录，含 manifest、topology、waypoints、index、validation | sample 位于 bringup 包 |
| 路网路径 | 必需 | `roadnet.package_path` 或 `ReloadRoadnet.package_path` | launch 配置提供 |
| 起终点 | 必需 | `PlanRoute` 中 start/goal node 或任务点字段 | 示例 `N0001 -> N0003` |
| 定位 | 控制必需 | `/localization/pose`，PoseStamped | 可配置，默认 `/localization/pose` |
| 轨迹 | 控制必需 | `/planning/trajectory` | 由规划节点发布 |
| 车辆状态 | 建议 | `/vehicle/state` | 可选输入 |
| 安全状态 | 建议 | `/safety/status` | estop 时最高优先级 |
| 底盘输出 | 必需 | `/yunle_chassis/control/scu_control_command` | 默认启用 |

## 12. 与已有文档的关系

建议阅读顺序：

1. `docs/PLANNING_LOCALIZATION_USAGE_AND_TECHNICAL_ROUTE.md`：先看本文，建立整体理解。
2. `docs/PLANNING_CONTROL_INPUT_GUIDE.md`：查看所有输入参数和 topic。
3. `docs/ROUTE_PLANNING_OPERATION_GUIDE.md`：查看如何用服务触发规划。
4. `docs/TECHNICAL_ROUTE_AND_DATAFLOW.md`：查看端到端数据流。
5. `docs/PLANNING_MODULE_CODE_WALKTHROUGH.md`：查看规划代码实现细节。
6. `docs/CONTROL_MODULE_CODE_WALKTHROUGH.md`：查看控制代码实现细节。
7. `docs/OPERATOR_STARTUP_CHECKLIST.md`：现场启动前按表检查。

## 13. 尚需人工验证的点

本文描述基于当前源码和配置，不代表已经在当前 Windows Codex 环境运行 ROS2。以下内容需要在真实 Ubuntu/ROS2 环境人工确认：

- `colcon build --symlink-install` 通过。
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py` 正常启动。
- `/planning/roadnet_status` 显示 ready。
- `PlanRoute` 服务返回 success。
- `/planning/trajectory` 持续或按规划请求发布。
- 外部定位 `/localization/pose` 发布频率满足超时要求。
- `/yunle_chassis/control/scu_control_command` 字段满足底盘协议。
- 急停、定位超时、轨迹超时会输出 brake true、speed 0、steering 0。
