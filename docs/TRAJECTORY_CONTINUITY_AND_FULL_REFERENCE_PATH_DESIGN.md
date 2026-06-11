# 轨迹连续性与完整参考路径设计

## 目标

保证语义目标规划时：

- `/planning/global_route` 表示完整拓扑路线。
- `/planning/full_reference_path` 表示完整、连续的几何参考路径。
- `/planning/trajectory` 表示控制模块实际跟踪的连续局部轨迹。

控制模块订阅 `/planning/trajectory`，因此该 topic 不允许出现几何跳段。

## 旧问题

旧流程会先按 `motion_planner.horizon_distance_m=15.0` 生成局部 trajectory，然后把远处语义目标所在 edge 的 cropped segment 追加到局部轨迹尾部。

错误形态：

```text
local horizon segment near vehicle
  -> directly appended far target-edge segment
  -> missing middle global-route edges
  -> /planning/trajectory has a geometric jump
```

这会让控制模块收到不可跟踪路径。

## 新流程

```text
PlanRoute / PlanMission request
  -> resolve start RoadnetAnchor
  -> resolve goal RoadnetAnchor
  -> global planner builds topology route
  -> build full reference path from full route edges
  -> append semantic target edge cropped segment continuously
  -> publish /planning/full_reference_path
  -> crop local trajectory from latest /localization/pose
  -> publish /planning/trajectory
```

## Topic 合同

| Topic | 类型 | 语义 | 消费方 |
|---|---|---|---|
| `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | 完整拓扑 node/edge 路线 | RViz、上位机、调试 |
| `/planning/full_reference_path` | `low_speed_av_interfaces/msg/Trajectory` | 完整连续几何参考路线 | RViz、诊断、人工验证 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制用连续局部轨迹 | `low_speed_av_control` |

## 参数

```yaml
planning:
  publish_full_reference_path: true
  full_reference_path_topic: "/planning/full_reference_path"
  local_trajectory_from_current_pose: true
  max_trajectory_point_jump_m: 2.0
```

`motion_planner.horizon_distance_m` 只用于从完整参考路径裁剪局部控制轨迹，不用于截断 full reference path。

## 语义目标拼接规则

1. task/parking/charging point 都解析为 RoadnetAnchor。
2. Anchor 保留语义点真实 `x/y/yaw`，不会只退化为 topology node。
3. 如果目标绑定 edge，则先规划到该 edge 的入口节点。
4. 再将目标 edge 从入口到语义点投影位置的 waypoint 段拼入 full reference path。
5. 如果当前处于 edge 的出口节点且允许倒车，则生成 reverse local segment。
6. 最后一个点替换为语义目标真实位姿，并设置 final stop。

## 连续性检查

规划节点对控制用局部 trajectory 执行相邻点距离检查：

```text
distance(point[i-1], point[i]) <= planning.max_trajectory_point_jump_m
```

默认阈值为 `2.0 m`。如果检查失败，规划服务返回失败并发布安全停车 trajectory，而不是把不连续路径交给控制模块。

## 仿真显示

`low_speed_av_simulation` 订阅：

- `/planning/global_route`
- `/planning/full_reference_path`
- `/planning/trajectory`
- `/localization/pose`

RViz 中建议区分：

- global route：橙色拓扑路线
- full reference path：黄色完整几何路径
- local trajectory：绿色控制轨迹
- vehicle pose：青色车辆箭头

## ROS2 命令

当前文档在 Windows Codex 环境生成，未执行 ROS2：

```text
SKIPPED_ROS2_UNAVAILABLE
```
