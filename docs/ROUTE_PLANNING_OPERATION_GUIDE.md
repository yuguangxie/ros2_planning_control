# 路线规划操作指南

本文说明如何让当前 planning module 开始规划。当前实现中，路线规划由 ROS2 服务 `/low_speed_av_planning/plan_route` 触发。

## 1. 真实 ROS2 环境启动

以下命令需要在真实 ROS2 环境中执行；当前 Windows Codex 环境未运行这些命令。

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
```

bringup launch 的默认行为：

- 启动节点名 `low_speed_av_planning`。
- 启动节点名 `low_speed_av_control`。
- 默认加载 `low_speed_av_bringup/config/planning_params.yaml`。
- 默认加载 `low_speed_av_bringup/config/control_params.yaml`。
- 默认将安装后的 `low_speed_av_bringup/sample_ad_package` 注入 `roadnet.package_path`。

证据：

- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:31` 声明 `roadnet_package_path`。
- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:32` 至 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:49` 启动两个节点。

## 2. 启动后确认

```bash
ros2 node list
ros2 topic list
ros2 service list
```

应看到：

```text
/low_speed_av_planning
/low_speed_av_control

/planning/global_route
/planning/trajectory
/planning/status
/planning/roadnet_status
/control/status
/yunle_chassis/control/scu_control_command

/low_speed_av_planning/reload_roadnet
/low_speed_av_planning/plan_route
/low_speed_av_planning/set_planner_algorithm
/low_speed_av_control/set_controller_algorithm
```

服务名来自 `~/reload_roadnet`、`~/plan_route`、`~/set_planner_algorithm`，节点名为 `low_speed_av_planning`，因此解析为上述绝对服务名。证据：`src/low_speed_av_planning/src/planning_node.cpp:81`、`src/low_speed_av_planning/src/planning_node.cpp:88`、`src/low_speed_av_planning/src/planning_node.cpp:95`。

## 3. 确认 roadnet 参数

```bash
ros2 param get /low_speed_av_planning roadnet.package_path
ros2 param get /low_speed_av_planning roadnet.reject_failed_validation
ros2 param get /low_speed_av_planning roadnet.verify_checksums
```

期望：

- `roadnet.package_path` 指向已解压的 AD Package 目录。
- `roadnet.reject_failed_validation=true`。
- `roadnet.verify_checksums=true`。

## 4. 重新加载 AD Package

实际服务字段来自 `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv`：

```text
string package_path
---
bool success
string package_id
string message
```

调用示例：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/ad_package'}"
```

使用 sample package 的示例：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/install/low_speed_av_bringup/share/low_speed_av_bringup/sample_ad_package'}"
```

成功后观察：

```bash
ros2 topic echo /planning/roadnet_status --once
ros2 topic echo /planning/status --once
```

成功期望：

- `success: true`
- `package_id` 非空
- `/planning/roadnet_status.ready: true`
- `/planning/status.state: active`

失败时常见原因：

- 缺少 `project_manifest.json`
- `validation.status == failed`
- `blocking_errors > 0`
- checksum mismatch
- `trajectory/waypoint_index.json` 越界
- waypoint 引用未知 edge

## 5. 切换规划算法

实际服务字段来自 `src/low_speed_av_interfaces/srv/SetPlannerAlgorithm.srv`：

```text
string global_planner_algorithm
string motion_planner_algorithm
string speed_planner_algorithm
---
bool success
string message
```

切换到 A* + reference_line + curvature：

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"
```

切换到 Dijkstra：

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'dijkstra', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"
```

如果某个字段留空，当前回调只更新非空字段。证据：`src/low_speed_av_planning/src/planning_node.cpp:432` 至 `src/low_speed_av_planning/src/planning_node.cpp:451`。

## 6. 触发路线规划

实际服务字段来自 `src/low_speed_av_interfaces/srv/PlanRoute.srv`：

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

### 6.1 使用节点 ID 规划

sample AD Package 可用示例：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

期望：

- response `success: true`
- response `message: ok`
- route 包含节点序列和边序列
- 同时发布 `/planning/global_route`
- 同时发布 `/planning/trajectory`

### 6.2 使用任务点作为目标

sample package 中 `semantics/task_points.json` 有 `T001`，链接到 `N0003`。可以调用：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'T001', goal_parking_point_id: ''}"
```

解析逻辑：若 `goal_node_id` 为空，`resolve_goal_node` 会查 `goal_task_point_id`，优先使用 `linked_node_id`，否则用 `linked_edge_id` 对应 edge 的 `to_node_id`。证据：`src/low_speed_av_planning/src/planning_node.cpp:272` 至 `src/low_speed_av_planning/src/planning_node.cpp:313`。

### 6.3 使用停车点作为目标

sample package 中 `semantics/parking_points.json` 有 `P001`，链接到 `E_L002_F`。可以调用：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: '', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: 'P001'}"
```

当前 srv 没有 `start_parking_point_id` 字段；起点可以使用 `start_node_id` 或 `start_task_point_id`。

## 7. 观察规划输出

```bash
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
ros2 topic echo /planning/status
ros2 topic echo /planning/roadnet_status
```

`/planning/global_route` 类型：`low_speed_av_interfaces/msg/GlobalRoute`

关键字段：

- `route_id`
- `source_package_id`
- `planner_algorithm`
- `node_ids`
- `edge_ids`
- `length_m`
- `estimated_time_s`
- `status`

`/planning/trajectory` 类型：`low_speed_av_interfaces/msg/Trajectory`

关键字段：

- `trajectory_id`
- `source_package_id`
- `planner_algorithm`
- `points[]`
- `emergency_stop`
- `status`

每个 `TrajectoryPoint` 包含 `x_m`、`y_m`、`yaw_rad`、`kappa_1pm`、`s_m`、`v_mps`、`gear`。

## 8. 规划失败行为

`on_plan_route` 失败时会：

1. response `success=false`。
2. 发布 `/planning/status`，状态为 failure。
3. 发布 emergency stop 类型的 failure trajectory。

证据：

- `src/low_speed_av_planning/src/planning_node.cpp:385` 是 PlanRoute 回调入口。
- `src/low_speed_av_planning/src/planning_node.cpp:397` 无法解析起终点时发布 failure trajectory。
- `src/low_speed_av_planning/src/planning_node.cpp:409` 全局路由失败时发布 failure trajectory。
- `src/low_speed_av_planning/src/planning_node.cpp:417` motion planner 输出空轨迹时发布 failure trajectory。
- `src/low_speed_av_planning/src/planning_node.cpp:159` 构造 failure stop trajectory。

## 9. 规划输出如何进入控制

控制节点订阅 `/planning/trajectory`。当 planning 成功发布轨迹后：

1. `ControlNode::on_trajectory` 将 ROS message 转为内部 `Trajectory`。
2. 控制定时器检查定位、轨迹、安全状态。
3. 当前 controller 计算 `desired_curvature_1pm`。
4. 车辆模型转换为 front/rear steering。
5. limiter/smoother/SCU mapper 生成底盘命令。

证据：

- `src/low_speed_av_control/src/control_node.cpp:222` 读取轨迹。
- `src/low_speed_av_control/src/control_node.cpp:285` 控制定时器。
- `src/low_speed_av_control/src/control_node.cpp:257` 计算 tracking command。
- `src/low_speed_av_control/src/control_node.cpp:351` 发布 command。
