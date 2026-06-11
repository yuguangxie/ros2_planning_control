# 规划输出数据合同

## 输出主题

| Topic | 类型 | 用途 | 频率语义 |
|---|---|---|---|
| `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | 完整 topology 路线、上位机显示、RViz 调试 | 默认 1 Hz 重发，可配置 |
| `/planning/full_reference_path` | `low_speed_av_interfaces/msg/Trajectory` | 完整连续几何参考路线，用于可视化和诊断 | 跟随最近一次成功规划缓存重发 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制模块真正跟踪的连续局部轨迹 | 默认 10 Hz 重发 |
| `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 规划状态和失败原因 | 事件触发 |
| `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | 路网加载状态 | transient local + 周期重发 |

## 控制输入合同

控制模块订阅 `/planning/trajectory`。规划服务只负责触发路线生成；成功后规划节点保存最近一次有效 full reference path，并按 `planning.trajectory_republish_rate_hz` 从最新定位附近裁剪连续局部 trajectory 重发，避免控制侧因为单次发布进入 `trajectory_timeout`。

默认配置：

```yaml
planning:
  republish_last_trajectory: true
  trajectory_republish_rate_hz: 10.0
  publish_full_reference_path: true
  full_reference_path_topic: "/planning/full_reference_path"
  local_trajectory_from_current_pose: true
  max_trajectory_point_jump_m: 2.0
```

## GlobalRoute、Full Reference Path 与 Trajectory 的区别

`GlobalRoute` 是 topology 级结果：

- node id 序列
- edge id 序列
- route 长度和估计时间
- planner algorithm

`FullReferencePath` 是完整几何级结果：

- 包含 global route 中所有 edge 的 waypoint
- 包含语义目标 edge 的 cropped segment
- 终点接近 task/parking/charging point 的真实 `x/y/yaw`
- 用于 RViz 显示完整路线和诊断连续性

`Trajectory` 是控制级结果：

- 从 full reference path 上按当前 `/localization/pose` 找最近点
- 向前裁剪 `motion_planner.horizon_distance_m`
- 保证相邻点距离不超过 `planning.max_trajectory_point_jump_m`
- 周期发布给控制模块跟踪

## 失败行为

以下失败必须发布安全停车 trajectory：

- roadnet 未加载
- start/goal 解析失败
- 当前定位缺失、过期或离路网太远
- 无可用全局 route
- semantic linked edge 无效
- motion planner 输出空 trajectory
- local trajectory 连续性检查失败

失败 trajectory：

```text
trajectory_id = failure_stop
emergency_stop = true
speed = 0
```

控制模块收到后应输出 SCU brake stop。

## ROS2 命令

本文档在 Windows Codex 环境更新，未执行 ROS2：

```text
SKIPPED_ROS2_UNAVAILABLE
```
