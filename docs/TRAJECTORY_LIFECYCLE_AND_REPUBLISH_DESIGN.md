# Trajectory Lifecycle And Republish Design

## 目标

修复 Ubuntu ROS2 验证中控制模块短暂输出后进入 `trajectory_timeout` 的问题。

原现象：

1. 操作员调用 `/low_speed_av_planning/plan_route`。
2. planning node 发布一次 `/planning/trajectory`。
3. control node 短时间内输出非零 SCU 命令。
4. 约 0.5 秒后，control node 因 `controller.trajectory_timeout_s=0.5` 进入安全停车：

```text
trajectory_timeout
```

## 最终设计

路线规划仍由服务触发：

```text
PlanRoute service -> 生成 GlobalRoute 和 Trajectory
```

但成功规划后，planning node 会缓存最近一次成功的：

- `GlobalRoute`
- `Trajectory`

并按参数周期重新发布，使 control node 持续收到新鲜轨迹。

## 新增参数

```yaml
planning:
  republish_last_route: true
  republish_last_trajectory: true
  route_republish_rate_hz: 1.0
  trajectory_republish_rate_hz: 10.0
  roadnet_status_publish_rate_hz: 1.0
```

默认行为：

- `/planning/trajectory`：10 Hz 重发最近一次有效规划轨迹。
- `/planning/global_route`：1 Hz 重发最近一次有效路线。
- `/planning/roadnet_status`：1 Hz 重发最近一次路网状态。

## 安全语义

周期重发不会绕过控制安全检查：

- localization timeout 仍由 control node 判定。
- safety estop 仍最高优先级。
- empty trajectory 仍安全停车。
- failure planning 会立即发布 `failure_stop`。
- failure planning 会停止继续重发旧的运动轨迹。

失败规划时，planning node 会缓存并重发安全停车轨迹，而不是继续重发旧的运动轨迹。

## Roadnet Status 可观测性

`/planning/roadnet_status` 现在使用 transient local QoS：

```text
depth=1
reliable
transient_local
```

同时按照 `planning.roadnet_status_publish_rate_hz` 周期重发最近状态。这样晚订阅的操作者更容易看到 `ready=true`。

## 推荐运行期检查

规划成功后检查轨迹频率：

```bash
ros2 topic hz /planning/trajectory
```

默认期望接近：

```text
10 Hz
```

检查控制输出：

```bash
ros2 topic hz /yunle_chassis/control/scu_control_command
ros2 topic echo /control/status
```

期望：

- control 不应仅因为 `/planning/trajectory` 单次发布而在 0.5 秒后进入 `trajectory_timeout`。
- 若 localization 中断或 safety estop 触发，仍应安全停车。

## 与控制 timeout 的关系

本修复没有把 `controller.trajectory_timeout_s` 简单改大。控制模块仍可用该参数判断轨迹输入是否新鲜。

trajectory 生命周期由 planning node 负责维护：

```text
服务触发生成轨迹 -> planning 周期重发 -> control 持续跟踪
```

