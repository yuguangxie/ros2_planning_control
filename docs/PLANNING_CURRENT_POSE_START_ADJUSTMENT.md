# 规划使用当前定位作为路线起点的调整说明

## 1. 旧行为

旧版 `PlanRoute` 需要显式提供起点，例如：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

如果 `start_node_id` 和 `start_task_point_id` 都为空，规划节点无法解析起点，会返回失败。

## 2. 新行为

现在规划节点订阅：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

当 `PlanRoute` 请求中 `start_node_id == ""` 且 `start_task_point_id == ""` 时，如果：

- `planning.use_current_pose_as_start=true`
- 最新 `/localization/pose` 未超时
- pose 距离路网 waypoint 足够近
- pose 航向与 waypoint 航向误差在阈值内

规划节点会自动把当前 pose 匹配到最近 waypoint/edge，并推断 start node。

新式调用：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

## 3. 兼容性

`PlanRoute.srv` 没有修改。旧字段仍然是：

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

兼容规则：

- 如果 `start_node_id` 非空，优先使用显式 start node。
- 如果 `start_task_point_id` 非空，优先解析显式 task point。
- 只有 start 字段为空时，才尝试使用当前定位。
- goal 仍然必须由 `goal_node_id`、`goal_task_point_id` 或 `goal_parking_point_id` 解析。

## 4. 新增参数

```yaml
planning:
  localization_timeout_s: 1.0
  use_current_pose_as_start: true
  start_match_max_distance_m: 3.0
  start_match_max_heading_error_rad: 1.57
  start_match_prefer_edge_projection: true
```

说明：

| 参数 | 含义 |
|---|---|
| `planning.localization_timeout_s` | 当前定位最大允许年龄 |
| `planning.use_current_pose_as_start` | start 为空时是否使用当前定位 |
| `planning.start_match_max_distance_m` | pose 到最近 waypoint 的最大距离 |
| `planning.start_match_max_heading_error_rad` | pose yaw 与 waypoint yaw 的最大误差 |
| `planning.start_match_prefer_edge_projection` | 匹配到 edge 中后段时是否优先使用该 edge 的 to node |

## 5. 匹配算法

当前实现是实用型 waypoint/edge matcher：

1. 检查 roadnet 是否加载。
2. 检查是否收到过 `/localization/pose`。
3. 检查 pose 接收时间是否超过 `planning.localization_timeout_s`。
4. 遍历 AD Package 中 `trajectory/waypoints.yaml` 的 waypoint。
5. 过滤航向误差超过阈值的 waypoint。
6. 选择欧氏距离最近的 waypoint。
7. 如果距离超过 `planning.start_match_max_distance_m`，返回失败。
8. 根据 waypoint 的 `edge_id` 找到 topology edge。
9. 根据 waypoint 在该 edge 的 index progress 推断 start node：
   - progress < 0.5：使用 edge.from node。
   - progress >= 0.5：使用 edge.to node。

这不是完整的连续边投影局部规划。它的目标是让路线规划能从“当前车辆附近的路网位置”实用地起步，同时保持全局规划仍然基于 topology node/edge。

## 6. 失败行为

以下情况会失败，并发布 failure status 和安全停车轨迹：

- roadnet package 未加载。
- 当前定位不存在。
- 当前定位超时。
- 当前定位包含非有限值。
- 最近 waypoint 距离超过阈值。
- 航向误差全部超过阈值。
- 匹配 waypoint 的 edge 不存在。
- goal 无法解析。
- 全局规划无可达路径。

典型错误信息：

```text
current localization pose is not available
current localization pose is stale: age=... timeout=...
nearest roadnet waypoint is too far: distance=... max=...
no waypoint matched localization heading threshold
```

## 7. 规划结果如何下发给控制模块

规划成功后仍然发布原有 topic：

```text
/planning/global_route
low_speed_av_interfaces/msg/GlobalRoute

/planning/trajectory
low_speed_av_interfaces/msg/Trajectory
```

控制模块不需要新增服务或手动接线。它原本就订阅：

```text
/planning/trajectory
```

因此“下发到控制模块”的实现方式是：规划节点发布 `/planning/trajectory`，控制节点接收后在控制周期中输出内部控制命令和 Yunle SCU 底盘命令。

最终底盘输出仍然是：

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

## 8. 推荐验证流程

1. 启动仿真定位和 planning/control：

   ```bash
   ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
     roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
     use_sim_pose:=true \
     pose_mode:=fixed_pose \
     launch_planning_control:=true \
     rviz:=true
   ```

2. 确认定位：

   ```bash
   ros2 topic echo /localization/pose
   ```

3. 空起点规划：

   ```bash
   ros2 service call /low_speed_av_planning/plan_route \
     low_speed_av_interfaces/srv/PlanRoute \
     "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
   ```

4. 显式起点回归验证：

   ```bash
   ros2 service call /low_speed_av_planning/plan_route \
     low_speed_av_interfaces/srv/PlanRoute \
     "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
   ```

两个调用都应成功，且对当前 fixed pose 默认位置，路线应从 `N0001` 开始。
