# 规划输出数据合同

## 输出主题

| 主题 | 类型 | 用途 | 频率语义 |
|---|---|---|---|
| `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | 拓扑 route、上位机显示、RViz 高亮、调试 | 默认 1 Hz 重发，可配置 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制模块真正跟踪的轨迹 | 默认 10 Hz 重发 |
| `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 规划状态和失败原因 | 事件触发 |
| `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | 路网加载状态 | transient local + 周期重发 |

## 控制输入合同

控制模块订阅 `/planning/trajectory`。规划服务只负责触发路线生成；成功后规划节点保存最近一次有效 trajectory，并按 `planning.trajectory_republish_rate_hz` 周期重发，避免控制侧因为单次发布而进入 `trajectory_timeout`。

默认配置：

```yaml
planning:
  republish_last_trajectory: true
  trajectory_republish_rate_hz: 10.0
```

## GlobalRoute 与 Trajectory 的区别

`GlobalRoute` 是 topology 级结果：

- node id 序列
- edge id 序列
- route 长度和估计时间
- planner algorithm

`Trajectory` 是控制级结果：

- 连续 waypoint / reference line
- `x/y/yaw/kappa/v_mps/s_m/gear`
- 终点停车状态
- 语义点 partial-edge 裁剪后的局部路径

对于 semantic goal，即使 `GlobalRoute` 由于 `start_node == goal_node` 表达为零长度，`Trajectory` 仍必须表达真实几何路径或 arrived stop。

## 失败行为

以下失败必须发布安全停车 trajectory：

- roadnet 未加载
- start/goal 解析失败
- 当前定位缺失、过期或离路网太远
- 无可用全局 route
- semantic linked edge 无效
- motion planner 输出空 trajectory

失败 trajectory：

- `trajectory_id = failure_stop`
- `emergency_stop = true`
- 速度为 0
- 控制模块收到后应输出 SCU brake stop。

## 可配置参数

```yaml
planning:
  arrival_radius_m: 0.5
  arrival_heading_tolerance_rad: 0.35
  semantic_goal_use_edge_projection: true
  semantic_goal_allow_reverse_local_segment: true
  semantic_goal_crop_waypoints: true
  semantic_goal_min_segment_length_m: 0.2
```

这些参数用于判断是否已到达语义点，以及是否允许生成反向局部裁剪段。
