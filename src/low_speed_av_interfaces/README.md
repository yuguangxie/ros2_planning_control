# low_speed_av_interfaces

## 模块定位
`low_speed_av_interfaces` 是低速自动驾驶规划/控制工作空间的接口包，只包含 ROS2 自定义消息和服务定义，不包含算法逻辑、节点逻辑或配置逻辑。

该包用于隔离跨模块通信合同：

- planning 发布 `GlobalRoute`、`Trajectory`、`RoadnetStatus`、`ModuleStatus`。
- control 订阅 `Trajectory`，发布 `ControlCommand`、`ModuleStatus`。
- bringup 和外部任务系统通过 service 调用 reload、plan route、切换算法。

## 文件结构

```text
low_speed_av_interfaces/
  CMakeLists.txt
  package.xml
  msg/
    TrajectoryPoint.msg
    Trajectory.msg
    GlobalRoute.msg
    ControlCommand.msg
    VehicleState.msg
    ModuleStatus.msg
    RoadnetStatus.msg
  srv/
    ReloadRoadnet.srv
    PlanRoute.srv
    SetPlannerAlgorithm.srv
    SetControllerAlgorithm.srv
```

## 消息说明

### TrajectoryPoint
单个轨迹点。字段采用 SI 单位：

- `x_m`、`y_m`：地图坐标，单位 m。
- `yaw_rad`：航向角，单位 rad。
- `kappa_1pm`：曲率，单位 1/m。
- `s_m`：路线累计里程。
- `v_mps`：目标速度，单位 m/s。
- `gear`：挡位提示，建议约定为 `0 UNKNOWN`、`1 DRIVE`、`2 REVERSE`、`3 PARK`。
- `behavior`：行为标签，例如 `follow`、`slow_down`、`obstacle_stop`。

### Trajectory
规划模块输出给控制模块的参考轨迹：

- `trajectory_id` 用于追踪一次轨迹发布。
- `source_package_id` 标识来源 AD Package。
- `planner_algorithm` 记录生成轨迹的算法。
- `emergency_stop` 表示轨迹是否为停车/失败轨迹。
- `status` 描述轨迹状态。

### GlobalRoute
全局路线消息：

- `node_ids`：拓扑节点序列。
- `edge_ids`：拓扑边序列。
- `length_m`：路线长度。
- `estimated_time_s`：估计通过时间。

### ControlCommand
控制输出消息：

- `speed_mps`、`acceleration_mps2`：目标速度和加速度。
- `steering_angle_rad`：兼容字段，默认等同前轮转角。
- `front_steering_angle_rad`：前轮转角。
- `rear_steering_angle_rad`：后轮转角，支持 dual Ackermann。
- `enable`：控制命令是否启用。
- `emergency_stop`：是否为急停/受控停车命令。
- `reason`：停车或异常原因。

### VehicleState
车辆状态输入，可选订阅。用于控制器估计当前速度、转角和自动驾驶状态。

### ModuleStatus
模块状态消息，建议 `level` 约定为：

- `0`：OK。
- `1`：WARN。
- `2`：ERROR。

### RoadnetStatus
规划模块加载 AD Package 后发布的 roadnet 摘要，包含 package id、schema version、节点/边/waypoint 数量和 ready 状态。

## 服务说明

### ReloadRoadnet
重新加载 AD Package。`package_path` 指向解压后的 AD Package 根目录。

### PlanRoute
请求全局路线规划。当前接口支持 node id，后续可以通过 task/parking point id 扩展任务目标。

### SetPlannerAlgorithm
运行时切换 global/motion/speed planner。

### SetControllerAlgorithm
运行时切换 controller algorithm 和 vehicle model。

## 设计约束
- 本包不得依赖 planning/control 的算法实现。
- 新增字段前应同步更新 `docs/03_ros2_interfaces.md`。
- `ControlCommand` 必须保留前/后轮转角字段，以支持 `front_ackermann` 和 `dual_ackermann`。

## 验证方式
当前 Codex 环境可能没有 ROS2，不强制运行 `colcon`。

有 ROS2 环境后可运行：

```bash
colcon build --packages-select low_speed_av_interfaces
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
```
