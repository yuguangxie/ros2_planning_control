# 操作员启动检查清单

本文给操作员或测试工程师使用，用于从冷启动到规划、控制、SCU 输出的人工确认。

## 1. 启动前

| 项目 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|
| AD Package 路径 | 路径存在且可读 |  |  |  |
| `project_manifest.json` | schema 为 `low_speed_roadnet_ad_package` |  |  |  |
| schema version | `1.1.0` 或兼容 `1.1.x` |  |  |  |
| validation | `status=passed`，`blocking_errors=0` |  |  |  |
| checksum | `checksums.sha256` 与 manifest hashes 一致 |  |  |  |
| canonical waypoints | 使用 `trajectory/waypoints.yaml` |  |  |  |
| waypoint index | `end_index_exclusive` 或兼容 inclusive `end_index` |  |  |  |
| chassis driver | Yunle ROS2 driver 已启动或准备启动 |  |  |  |
| `chassis_interfaces` | ROS2 环境可找到该包 |  |  |  |
| 急停 | 机械/电气 E-stop 可用 |  |  |  |
| 测试环境 | 台架、车轮离地或封闭低速场地 |  |  |  |
| 默认速度限制 | 保持低速，如 0.5 m/s 或项目配置值 |  |  |  |

## 2. 启动命令

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
```

覆盖 AD Package：

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/ad_package
```

## 3. 启动后系统检查

| 项目 | 命令 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|---|
| 节点 | `ros2 node list` | `/low_speed_av_planning`、`/low_speed_av_control` |  |  |  |
| 服务 | `ros2 service list` | planning/control 服务可见 |  |  |  |
| 话题 | `ros2 topic list` | planning/control/SCU topic 可见 |  |  |  |
| roadnet 参数 | `ros2 param get /low_speed_av_planning roadnet.package_path` | 指向目标 package |  |  |  |
| 规划算法 | `ros2 param get /low_speed_av_planning global_planner.algorithm` | `astar` 或预期值 |  |  |  |
| 定位 topic | `ros2 param get /low_speed_av_control topics.localization_pose_topic` | `/localization/pose` 或预期值 |  |  |  |
| 控制算法 | `ros2 param get /low_speed_av_control controller.algorithm` | `lqr` 或预期值 |  |  |  |
| 车辆模型 | `ros2 param get /low_speed_av_control vehicle.model` | `front_ackermann` 或 `dual_ackermann` |  |  |  |
| SCU topic | `ros2 param get /low_speed_av_control topics.scu_command_topic` | `/yunle_chassis/control/scu_control_command` |  |  |  |

## 4. Roadnet 加载检查

```bash
ros2 topic echo /planning/roadnet_status --once
ros2 topic echo /planning/status --once
```

| 项目 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|
| roadnet ready | `ready: true` |  |  |  |
| package_id | 非空 |  |  |  |
| nodes/edges/waypoints | 数量大于 0 |  |  |  |
| validation_status | `passed` 或明确 ready 状态 |  |  |  |
| planning status | `active` 或可规划状态 |  |  |  |

若未加载，执行：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/ad_package'}"
```

## 5. 移动前安全检查

| 项目 | 操作 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|---|
| Estop 触发 | 发布 `/safety/status` estop | SCU brake true、speed 0、steering 0 |  |  |  |
| Estop OK 心跳 | 发布 `state=ok level=0` | 保持 `ESTOP_LATCHED`，不得自动恢复 |  |  |  |
| Estop 显式清除 | 调用 `/low_speed_av_control/clear_estop` | 条件满足时先进入 `READY`；条件不足返回原因 |  |  |  |
| VehicleState 门控 | 依次测试自治关闭、人工制动、故障码 | 均为零速、制动停车 |  |  |  |
| 定位超时 | 停止 `/localization/pose` | controlled stop |  |  |  |
| 轨迹超时 | 停止 `/planning/trajectory` | controlled stop |  |  |  |
| 空轨迹 | 发布空 trajectory | controlled stop |  |  |  |
| SCU topic | echo SCU | shift 只为 1/2/3 |  |  |  |
| D gear | forward trajectory | shift=1，speed 非负 |  |  |  |
| R gear | reverse trajectory | shift=3，speed 非负 |  |  |  |
| 转角方向 | 小角度指令 | 车辆实际方向与配置一致 |  |  |  |
| brake | 安全停车 | 底盘停止 |  |  |  |

## 6. 路线规划操作

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

确认：

| 项目 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|
| PlanRoute response | `success: true` |  |  |  |
| route edge ids | 非空且符合预期 |  |  |  |
| route node ids | 从起点到终点 |  |  |  |
| trajectory points | 非空，字段有限 |  |  |  |
| trajectory speed | 低速且不超过限制 |  |  |  |
| trajectory gear | 与路线方向一致 |  |  |  |

## 7. 控制输出检查

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic echo /control/status
```

如果 `output.mode=both` 或 `internal`：

```bash
ros2 topic echo /control/command
```

锁存急停清除命令：

```bash
ros2 service call /low_speed_av_control/clear_estop std_srvs/srv/Trigger "{}"
```

执行前必须确认最新 safety 不再请求急停、定位和轨迹新鲜、VehicleState 新鲜、车辆已静止、无故障、未踩制动且自治已许可。软件检查不能替代机械急停和底盘硬件 watchdog；实车测试前必须单独确认后者有效。

| 项目 | 期望结果 | 观察结果 | Pass/Fail | 备注 |
|---|---|---|---|---|
| SCU message type | `chassis_interfaces/msg/ScuControlCommand` |  |  |  |
| speed unit | km/h，非负 |  |  |  |
| steering unit | deg |  |  |  |
| shift | 只为 1/2/3 |  |  |  |
| normal brake | 正常跟踪时 false |  |  |  |
| safety brake | 安全停车时 true |  |  |  |
| lights | 默认 0 或按配置 |  |  |  |
| valid flags | 默认 false 或按配置 |  |  |  |

## 8. 结束测试

1. 触发 Estop。
2. 确认 SCU brake stop。
3. 停止 launch。
4. 保存以下日志或截图：
   - `ros2 service call PlanRoute` 输出。
   - `/planning/global_route`。
   - `/planning/trajectory`。
   - `/control/status`。
   - `/yunle_chassis/control/scu_control_command`。
   - chassis driver 或 CAN 工具输出。

## 9. 人工确认签名

| 角色 | 姓名 | 日期 | 结论 | 备注 |
|---|---|---|---|---|
| 操作员 |  |  |  |  |
| 安全员 |  |  |  |  |
| 软件负责人 |  |  |  |  |
| 底盘负责人 |  |  |  |  |
