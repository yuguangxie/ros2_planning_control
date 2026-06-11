# 仿真可视化模块使用说明

## 1. 模块目的

`low_speed_av_simulation` 是当前工程新增的 ROS2/ament 仿真与可视化包，用于在 RViz 中查看路网、当前位置、规划路线和规划轨迹，并提供一个明确命名的模拟定位发布节点。

它解决的问题：

- 加载 `roadnet_ad_package_20260610T012525Z` 或其他 AD Package v1.1 目录。
- 可视化 topology node、edge、waypoint/reference line、语义区域、任务点、停车点、充电点、route point。
- 发布模拟 `/localization/pose`，使规划节点可以用当前 pose 推断路线起点。
- 订阅 `/planning/global_route` 和 `/planning/trajectory`，在 RViz 中高亮显示规划结果。
- 保持控制模块原有输入不变，控制仍然通过 `/planning/trajectory` 接收轨迹。

## 2. 新增节点

### 2.1 `roadnet_visualization_node`

输入：

- `roadnet.package_path`
- `/planning/global_route`
- `/planning/trajectory`
- `/localization/pose`

输出：

```text
/simulation/roadnet_markers
visualization_msgs/msg/MarkerArray

/simulation/route_markers
visualization_msgs/msg/MarkerArray

/simulation/trajectory_path
nav_msgs/msg/Path

/simulation/vehicle_markers
visualization_msgs/msg/MarkerArray
```

显示内容：

- topology nodes：蓝色球点。
- topology edges：青色线段。
- waypoints/reference line：浅色线。
- semantic areas：drivable/no-go/speed-zone 多边形边界。
- task/parking/charging/route points：不同颜色点。
- current vehicle pose：箭头。
- planned route：高亮线。
- planned trajectory：绿色路径。

### 2.2 `sim_localization_pose_publisher_node`

输出：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

服务：

```text
/simulation/start
std_srvs/srv/Trigger

/simulation/pause
std_srvs/srv/Trigger

/simulation/reset
std_srvs/srv/Trigger
```

模式：

- `fixed_pose`：固定发布配置的 x/y/yaw。
- `path_follow`：默认模式。未收到规划路径前发布初始 pose，收到 `/planning/full_reference_path` 后沿完整路径移动；若 full reference 不可用则 fallback 到 `/planning/trajectory`。
- `trajectory_replay`：订阅 `/planning/trajectory` 后沿规划轨迹回放；没有规划轨迹时会尝试回退到路网 waypoint。
- `roadnet_waypoint_replay`：直接沿 AD Package 中的 waypoint 回放。

## 3. 真实 ROS2 环境启动命令

当前 Windows Codex 环境未检测到 `ros2` 和 `colcon`，以下命令需要在 Ubuntu/ROS2 工作空间中运行。

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
```

只启动可视化和模拟定位：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  rviz:=true
```

启动可视化、模拟定位，并一起启动 planning/control：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  launch_planning_control:=true \
  rviz:=true
```

仅可视化，不发布模拟定位：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=false \
  rviz:=true
```

## 4. 常用参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `roadnet_package_path` | bringup sample package | AD Package 目录 |
| `use_sim_pose` | `true` | 是否启动模拟定位节点 |
| `pose_mode` | `path_follow` | `path_follow` / `fixed_pose` / `trajectory_replay` / `roadnet_waypoint_replay` |
| `publish_rate_hz` | `20.0` | 模拟定位发布频率 |
| `frame_id` | `map` | RViz 和 PoseStamped frame |
| `start_paused` | `false` | 模拟定位是否启动后暂停 |
| `launch_planning_control` | `false` | 是否包含原 planning/control bringup |
| `rviz` | `true` | 是否启动 RViz |

## 5. 操作流程

### 5.1 确认模拟定位

```bash
ros2 topic echo /localization/pose
```

期望：

- 能看到 `geometry_msgs/msg/PoseStamped`。
- `header.frame_id` 为 `map`。
- orientation quaternion 有效。
- 发布频率接近 `publish_rate_hz`。

### 5.2 确认路网可视化

```bash
ros2 topic echo /simulation/roadnet_markers
```

RViz 中应看到：

- 路网节点。
- 路网边。
- waypoint/reference line。
- 语义区域边界。
- 任务点。
- 当前模拟车辆箭头。

### 5.3 控制模拟定位

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
```

### 5.4 使用当前 pose 触发路线规划

先确认 planning/control 已启动：

```bash
ros2 topic echo /planning/roadnet_status
ros2 service list
```

然后调用空起点规划：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

规划节点会在 `planning.use_current_pose_as_start=true` 时使用最新 `/localization/pose` 匹配最近路网 waypoint/edge，并推断 start node。

### 5.5 观察规划输出

```bash
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /planning/status
```

RViz 中应看到：

- planned global route 高亮线。
- planned trajectory 绿色路径。
- route start/goal marker。

### 5.6 观察控制输出

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command
```

注意：

- 该 topic 是真实底盘控制命令。
- 如果连接真实底盘，必须保证车辆处于台架、轮离地或底盘禁用的安全环境。

## 6. 常见问题

| 问题 | 可能原因 | 处理 |
|---|---|---|
| RViz 看不到路网 | `roadnet_package_path` 错误或 checksum mismatch | 检查绝对路径和 `/planning/roadnet_status` |
| `/localization/pose` 没输出 | `use_sim_pose=false` 或 `start_paused=true` | 开启模拟定位或调用 `/simulation/start` |
| 空起点规划失败 | 没有最新 pose、pose 超时、pose 距离路网太远 | echo `/localization/pose`，调大匹配距离或设置 fixed pose 到路网附近 |
| 规划成功但控制无命令 | 控制节点未启动、轨迹超时、安全状态 estop | echo `/control/status` 和 `/planning/trajectory` |
| speed-zone/no-go 看得到但不影响路线 | 当前区域没有覆盖 waypoint | 使用覆盖 waypoint 的语义测试包 |

## 7. 安全提示

- `sim_localization_pose_publisher_node` 是模拟定位节点，不应在真实车辆运动环境中替代真实定位。
- 如果同时启动 control 和真实 chassis driver，模拟定位可能导致车辆输出真实 SCU 控制命令。
- 实车联调时必须有 E-stop 和操作员，建议先关闭底盘使能或轮离地。
