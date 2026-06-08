# 00 Context and Assumptions

## 项目背景

当前路网编辑器已经能够导出供规划和控制算法使用的 Low Speed Roadnet AD Package ZIP。该 ZIP 是本项目自定义的 `roadnet/topology/waypoints` 自动驾驶输入包，不是 Nav2、Lanelet2，也不是前端 `.roadnet` 编辑工程文件。

下游目标是在 ROS2 中构建两个独立模块：

```text
low_speed_av_planning
low_speed_av_control
```

其中规划模块消费 AD Package，输出全局路线和局部轨迹；控制模块消费轨迹、定位、底盘状态和安全状态，输出阿克曼底盘控制命令。

## 关键假设

1. 坐标系为 `map`。
2. 距离单位为 `m`。
3. 角度单位为 `rad`。
4. 速度单位为 `m/s`，字段名使用 `mps`。
5. 曲率单位为 `1/m`。
6. 控制参考点为 `rear_axle`。
7. 当前 AD Package schema 版本为 `1.1.0`。
8. 当前 AD Package 不支持非零 `origin.yaw`；如果 validation failed，车端规划模块必须拒绝使用。
9. 定位模块已完成，定位输出默认 topic 是 `/localization/pose`，但必须可在配置文件中修改。
10. 底盘是四轮阿克曼，需要支持前轮转向阿克曼和前后双转阿克曼。
11. Codex 运行环境没有 ROS2，因此不能要求 Codex 执行 `colcon build`。

## 总体技术路线

```text
AD Package ZIP
  -> RoadnetLoader
  -> topology graph
  -> Dijkstra/A* global route
  -> waypoint_index + waypoints.yaml trajectory stitching
  -> reference_line motion planning
  -> speed planning
  -> /planning/trajectory
  -> selectable controller
  -> front_ackermann or dual_ackermann vehicle model
  -> /control/command
```

## 分包原则

- `low_speed_av_planning` 不直接输出底盘命令。
- `low_speed_av_control` 不直接运行全局规划。
- `low_speed_av_interfaces` 不依赖规划/控制实现。
- `low_speed_av_bringup` 不包含算法实现。
