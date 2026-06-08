# Goal 02 — Generate Control Module Only

请只生成或修改：

```text
src/low_speed_av_interfaces
src/low_speed_av_control
src/low_speed_av_bringup 中与 control 有关的 config/launch
scripts 中 control 离线检查
```

控制模块不直接读取 AD Package ZIP，运行时消费 `/planning/trajectory`。离线 demo 可以读取 sample `trajectory/waypoints.yaml` 生成测试 trajectory。

必须支持：

```text
controller.algorithm = pure_pursuit | stanley | lqr | mpc_sampler
vehicle.model = front_ackermann | dual_ackermann
localization_pose_topic default = /localization/pose and configurable
```

当前环境没有 ROS2，不要求运行 colcon。运行纯 Python 离线检查并写报告。
