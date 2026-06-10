# `/localization/pose` 模拟定位指南

## 1. Topic 定义

当前规划/控制链路使用的定位输入是：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

控制节点订阅该 topic；规划节点现在也订阅该 topic，用于在 `PlanRoute` 请求起点为空时推断当前路线起点。

## 2. 坐标系要求

推荐：

- `header.frame_id = "map"`。
- x/y 单位为米。
- yaw 通过 quaternion 表达。
- 车辆参考点尽量与 AD Package 中 `control_reference_frame: rear_axle` 一致。

如果定位发布的是 base_link 或车辆中心点，建议外部定位模块先转换到 rear axle，再发布给当前规划/控制链路。

## 3. 模拟定位节点

节点：

```text
sim_localization_pose_publisher_node
```

默认输出：

```text
/localization/pose
```

服务：

```text
/simulation/start
/simulation/pause
/simulation/reset
```

## 4. fixed_pose 模式

用途：

- 固定当前位置，便于测试“空起点 PlanRoute 使用当前 pose”。
- 推荐初次联调使用。

启动：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose
```

默认 fixed pose 与当前路网首个 waypoint 附近对齐：

```text
x = 0.554
y = 1.473
yaw = -0.9178
```

## 5. trajectory_replay 模式

用途：

- 规划出 `/planning/trajectory` 后，让模拟定位沿轨迹运动。
- 用于观察控制节点在轨迹跟踪过程中的输出。

启动：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=trajectory_replay \
  launch_planning_control:=true
```

行为：

- 订阅 `/planning/trajectory`。
- 收到轨迹后从轨迹起点开始回放。
- 如果尚未收到轨迹，会尝试使用路网 waypoint 作为 fallback。

## 6. roadnet_waypoint_replay 模式

用途：

- 不依赖规划结果，直接沿 AD Package 中的 waypoint 回放定位。
- 适合验证 RViz、定位 topic、控制 timeout 行为。

启动：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=roadnet_waypoint_replay
```

## 7. 推荐频率和 timeout

默认：

```text
publish_rate_hz = 20.0
planning.localization_timeout_s = 1.0
controller.localization_timeout_s = 0.2
```

建议：

- 仿真定位频率不低于 20 Hz。
- 如果调试时系统负载较高，可临时调大控制的 `controller.localization_timeout_s`。
- 真实车辆不要依赖模拟定位，除非底盘禁用或处于安全台架。

## 8. 示例命令

查看定位：

```bash
ros2 topic echo /localization/pose
```

暂停：

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
```

启动：

```bash
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
```

复位：

```bash
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
```

手工发布一个 PoseStamped：

```bash
ros2 topic pub --rate 20 /localization/pose geometry_msgs/msg/PoseStamped \
  "{header: {frame_id: 'map'}, pose: {position: {x: 0.554, y: 1.473, z: 0.0}, orientation: {x: 0.0, y: 0.0, z: -0.4429, w: 0.8966}}}"
```

## 9. 与控制模块的关系

控制节点需要同时满足：

- `/localization/pose` 新鲜。
- `/planning/trajectory` 新鲜且非空。
- 没有 `/safety/status` estop。

如果定位停止或超时，控制应输出安全停车命令，最终映射为：

```text
/yunle_chassis/control/scu_control_command
scu_brake_enable=true
scu_target_speed=0
front/rear steering=0
```
