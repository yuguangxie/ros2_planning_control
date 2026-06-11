# start==goal 但几何未到达的处理

## 问题

Ubuntu 复测发现：

```text
current pose -> RP-003
```

当前定位匹配到 `N0001`。`RP-003` 关联边为 `E_L-007_F`，拓扑方向 `N0013 -> N0001`，旧 fallback 将目标也解析为 `N0001`，形成：

```text
route_N0001_N0001
edge_ids=[]
```

旧 motion planner 对空 edge 序列输出空 trajectory，服务返回 `motion planner produced empty trajectory`。

## 修复规则

当 `start_node == goal_node` 时，不能只看 topology node，必须先做几何到达判断：

1. 当前 pose 到 goal anchor 距离小于 `planning.arrival_radius_m`。
2. 当前 yaw 与 goal yaw 的误差小于 `planning.arrival_heading_tolerance_rad`。

只有同时满足才返回 `success=true` + `arrived_stop` trajectory。

如果几何未到达：

- 若目标点在同一 edge 且可以正向到达，裁剪正向 waypoint。
- 若当前在 edge `to_node`，目标在 edge 中段，且允许 reverse local segment，则从 edge 末端反向裁剪到目标点。
- 若不允许 reverse，则应尝试绕行到 edge `from_node` 后再沿 edge 正向裁剪。

## RP-003 期望

默认参数允许 reverse local segment：

```yaml
planning:
  semantic_goal_allow_reverse_local_segment: true
```

因此 `current pose -> RP-003` 应输出：

- service `success=true`
- 不再因为 `route_N0001_N0001` 失败
- `/planning/trajectory` 非空
- trajectory 最后一点接近 `RP-003` 的 `x/y/yaw`
- 局部段 gear 为 reverse

## 到达判定

如果模拟定位已经放在 `RP-003` 附近，则应返回成功的停止轨迹，而不是 failure stop：

```text
status: arrived_stop
speed: 0
emergency_stop: false
```

## 兼容性

显式 node-id 规划仍按 topology route 工作：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0005', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```
