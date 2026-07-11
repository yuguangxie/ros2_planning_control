# ROS2 集成测试计划

## 已注册的生产链路测试

`low_speed_av_bringup/test/test_planning_control_safety_launch.py` 使用 `launch_testing` 启动真实 `planning_node` 和 `control_node`，加载 canonical sample，调用无效 `PlanRoute`，并断言：

1. Planning service 返回失败；
2. `/planning/trajectory` 发布 `emergency_stop=true`；
3. `/control/command` 为零速、制动、disable 且 reason 非空；
4. `/yunle_chassis/control/scu_control_command` 为零 target speed、brake enable。

所有等待使用 monotonic deadline 和显式 timeout；失败信息包含已收到的 trajectory/internal/SCU 消息数量。测试不启动 Chassis Driver、不打开 UDP、不访问真实网关。

Chassis publisher-loss watchdog 规格保留为跳过测试：

```text
SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002
```

它不能计为 PASS，因为生产 driver 尚无 scheduler/timeout watchdog。

## 后续 integration case

- canonical sample ready RoadnetStatus；
- PlanRoute/PlanMission 成功 route 与 trajectory；
- 四 controller emergency 行为一致；
- localization/trajectory timeout；
- safety estop，普通 OK 不清 latch，Trigger clear 前置条件；
- VehicleState disabled/fault/brake；
- late subscriber 与 QoS；
- 修复 CDX-P0-002 后再启用 Chassis publisher-loss stop。

## 执行命令

```bash
source /opt/ros/humble/setup.bash
colcon build --symlink-install
source install/setup.bash
colcon test --packages-select low_speed_av_bringup --event-handlers console_direct+
colcon test-result --verbose
```

当前 Windows 环境没有 ROS2，因此本文件描述的是 expected procedure；当前 observed result 为 `SKIPPED_ROS2_UNAVAILABLE`，不是 PASS。

## HIL/实车边界

launch test 只验证 ROS2 消息链路。真实车辆仍必须独立验证 CAN capture、硬件 watchdog、制动距离、转角方向、DDS/Control/Driver 崩溃和断电；在 `CDX-P0-002` 关闭前不得据此放行实车运动测试。
