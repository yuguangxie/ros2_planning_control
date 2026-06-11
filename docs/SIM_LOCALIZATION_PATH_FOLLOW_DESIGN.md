# 仿真定位 path_follow 设计

## 目标

`sim_localization_pose_publisher_node` 负责发布仿真 `/localization/pose`。启动后，在没有规划结果前持续发布配置中的初始 pose；人工通过 `/plan_mission` 或 `/plan_route` 下发目标后，仿真节点订阅规划输出并沿路径移动 pose，形成：

```text
仿真定位 -> planning current pose start -> full reference path / trajectory -> control -> SCU command
```

该能力只属于仿真模块，不让 planning 节点发布定位。

## 模式

支持模式：

| mode | 行为 |
|---|---|
| `fixed_pose` | 只发布初始 pose，不随路径移动。 |
| `path_follow` | 默认模式。先发布初始 pose，收到路径后沿路径连续移动。 |
| `trajectory_replay` | 兼容旧模式；可沿接收到的 trajectory 或 fallback 路网路径移动。 |
| `roadnet_waypoint_replay` | 保留旧功能，沿 AD Package waypoint 顺序回放。 |

## 初始 Pose 配置

推荐配置：

```yaml
simulation:
  localization:
    enabled: true
    pose_topic: "/localization/pose"
    publish_rate_hz: 20.0
    frame_id: "map"
    mode: "path_follow"
    initial_pose:
      source: "explicit"
      x: 0.554
      y: 1.473
      yaw: -0.9178
      waypoint_id: ""
      task_point_id: ""
      edge_id: ""
      edge_progress: 0.0
```

`source` 支持：

- `explicit`：直接使用 `x/y/yaw`。
- `waypoint`：通过 `waypoint_id` 从路网 waypoint 初始化。
- `task_point`：通过 `task_point_id` 从 task/parking/charging 语义点初始化。
- `edge_progress`：通过 `edge_id + edge_progress` 从 edge 上指定进度初始化。

如果配置非法，节点会输出 warning，并 fallback 到 explicit 或第一个可用 roadnet waypoint。

## 路径输入优先级

仿真节点订阅：

```text
/planning/full_reference_path
low_speed_av_interfaces/msg/Trajectory

/planning/trajectory
low_speed_av_interfaces/msg/Trajectory
```

默认：

```yaml
simulation:
  localization:
    follow:
      follow_source: "full_reference_path"
      fallback_to_local_trajectory: true
```

原因：

- `/planning/full_reference_path` 是完整连续几何参考路线，更适合驱动仿真 pose 走完整任务。
- `/planning/trajectory` 是控制用局部轨迹，适合作为 fallback，也用于接收 `failure_stop / emergency_stop` 停止信号。

## 去重与新路径

规划节点会周期重发 path/trajectory，因此仿真节点不能每收到一条消息都从起点开始。节点使用路径内容签名去重，签名包含：

- source package
- status/emergency_stop
- 点数量
- 首末点 waypoint/edge/坐标
- 路径长度

同一条路径重复发布时，仿真节点忽略，不重置 progress。

新路径到来时，默认：

```yaml
reanchor_on_new_path: true
restart_on_new_path: false
```

含义是：从当前 pose 在新路径上的最近投影点继续跟随，不跳回路径第一个点。

## 连续移动

节点将 trajectory points 转换为 polyline。每个 publish tick：

1. 根据 `dt`、速度和加速度限制更新路径弧长 `s`。
2. 在线段之间线性插值 `x/y`。
3. 优先使用路径点 yaw 插值；若 yaw 非法，则用相邻点方向。
4. 发布规范化 quaternion。
5. 到达终点后保持终点 pose，并发布 `arrived` 状态。

reverse gear 路径按已发布路径点顺序移动。仿真节点不决定是否倒车；倒车是否允许由 planning 配置决定。

## 状态输出

新增/完善：

```text
/simulation/status
low_speed_av_interfaces/msg/ModuleStatus

/simulation/pose_path
nav_msgs/msg/Path
```

状态包括：

- `waiting_for_path`
- `following_path`
- `paused`
- `arrived`
- `holding_failure_stop`
- `invalid_path`
- `reset`

`/simulation/status.message` 会包含当前 source、trajectory id、progress、总长、点数、endpoint distance 等调试信息。

## 安全说明

这是理想 path-follow 仿真，不等同真实车辆动力学。它用于验证规划、可视化、topic wiring 和控制输入新鲜度。如果未来要验证控制器动力学，应新增基于 `/control/command` 或 SCU command 的车辆模型，而不是用 path_follow 替代真实闭环动力学。

