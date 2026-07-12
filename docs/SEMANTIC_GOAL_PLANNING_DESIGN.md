# 语义目标点规划设计

## 目标

当前项目的业务任务推荐使用语义目标点，而不是直接使用 topology node：

- `goal_task_point_id`
- `goal_parking_point_id`
- `goal_type: charging` + `goal_id`，通过新增 `/low_speed_av_planning/plan_mission`

`start_node_id` / `goal_node_id` 仍保留，用于调试、拓扑回归测试和兼容旧客户端。

## 为什么不应只规划到 node

路网编辑器导出的 task point 通常绑定在 edge 中段，例如 `RP-003`：

- 语义点坐标：`roadnet_ad_package_20260610T012525Z/semantics/task_points.json`
- 关联边：`E_L-007_F`
- 该边拓扑：`N0013 -> N0001`

如果只把目标 fallback 到 `to_node_id=N0001`，当当前定位也匹配到 `N0001` 时，服务会形成 `route_N0001_N0001`。拓扑 route 可以为空，但车辆与 `RP-003` 的几何位置并不相同，因此不能直接视为已到达，也不能输出空 trajectory。

## RoadnetAnchor

规划节点新增内部锚点结构 `RoadnetAnchor`，代码位置：

- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`

锚点保留：

- 目标类型：current pose / node / edge point / task / parking / charging
- 语义点 ID
- fallback node
- linked edge、from node、to node
- 语义点 `x/y/yaw`
- 投影 waypoint index
- `s_on_edge` / edge progress
- 是否要求终点停车

## 解析规则

1. 有效 `linked_node_id` 优先使用，但不会丢弃语义点几何位置。
2. `linked_node_id` 缺失、空、`null` 或无效时，使用 `linked_edge_id`。
3. goal 语义点默认 fallback 到 edge `to_node_id`。
4. start 语义点默认 fallback 到 edge `from_node_id`。
5. 语义点会投影到 linked edge 的 waypoint 序列，用于裁剪局部 trajectory。
6. 无效 semantic point 会清晰失败并发布 `failure_stop`。

## 服务入口

兼容服务仍然可用：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: '', start_task_point_id: '', goal_task_point_id: 'RP-003', goal_parking_point_id: ''}"
```

新增业务级服务：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-003'}"
```

`PlanMission` 支持：

- `start_type`: `""`, `current_pose`, `node`, `task`, `parking`, `charging`
- `goal_type`: `node`, `task`, `parking`, `charging`

## 与控制模块的关系

语义目标点规划最终仍通过 `/planning/trajectory` 下发给控制模块。`/planning/global_route` 是拓扑路线和可视化调试信息，不是控制模块的直接跟踪输入。

## 当前限制

- `PlanRoute.srv` 没有 `goal_charging_point_id` 字段，因此 charging 目标通过新增 `PlanMission.srv` 使用。
- parking/charging 成功路径需要带相应语义点的 AD Package 或离线 fixture 验证；当前正式包 parking/charging 为空。

## Phase 15 production helper

Semantic 解析已迁入 production `planning_helpers`。空字符串、`null`、`"null"`、`none` 均不作为 node ID；有效 `linked_node_id` 优先，未知或空 node 可由合法 `linked_edge_id` fallback。Task、parking、charging 共用 resolver，terminal segment、final stop 和诊断使用同一生产实现。
