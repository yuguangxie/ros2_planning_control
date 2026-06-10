# 仿真、规划与控制专项验证报告

日期：2026-06-10

本报告是 `reports/ubuntu_ros2_runtime_validation_report.md` 的专项摘要，重点记录仿真可视化、规划、当前定位起点、控制和 SCU 输出行为。

原始日志目录：`reports/runtime_validation_logs/`

## 1. 仿真验证

启动命令：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/home/xie/planning_control/ros2_planning_control/ros2_planning_control/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

结果：

- `/roadnet_visualization_node` 启动成功。
- `/sim_localization_pose_publisher` 启动成功。
- `/rviz2` 进程启动成功，OpenGL 4.6 初始化成功。
- 本次未做人工 RViz 画面截图或目视确认，因此 RViz 画面内容标记为“进程可启动，视觉内容未人工确认”。
- `/localization/pose` 持续发布，频率约 20 Hz。
- `/localization/pose.header.frame_id` 为 `map`。
- pose quaternion 范数为 1.0，有效。
- `/simulation/roadnet_markers` 非空，捕获到 12 个 marker。
- `/simulation/vehicle_markers` 非空，捕获到 1 个 marker。
- 规划后 `/simulation/route_markers` 非空，捕获到 3 个 marker。
- 规划后 `/simulation/trajectory_path` 非空，捕获到 67 个 pose，frame 为 `map`。
- `/simulation/pause`、`/simulation/start`、`/simulation/reset` 均返回 `success=True`。

## 2. 规划验证

成功用例：

| 用例 | 结果 |
|---|---|
| `N0001 -> N0002` | 成功，edge 为 `E_C-001_F`，轨迹 11 点 |
| `N0001 -> N0005` | 成功，4 条 edge，轨迹 67 点 |
| 空起点 -> `N0003` | 成功，当前 pose 匹配到 `N0001`，轨迹起点距离当前 pose 为 0.0 m |
| `N0015 -> N0014` | 成功，reverse edge 为 `E_C-007_R`，trajectory gear 为 2 |

失败路径：

| 用例 | 结果 |
|---|---|
| 无效 goal node | 失败信息清晰，发布 `failure_stop`，`emergency_stop=true` |
| 暂停仿真导致定位超时 | 失败信息清晰，包含 stale pose age 和 timeout |
| 无效 task point | 失败信息清晰，发布安全停车轨迹 |
| 无效 parking point | 失败信息清晰，发布安全停车轨迹 |

任务点成功用例：

- 当前包存在真实 task point：`RP-001` 到 `RP-007`。
- `goal_task_point_id: RP-001` 和 `start_task_point_id: RP-003` 均失败。
- 失败信息为 `start or goal node is not in topology`。
- 轨迹输出为 `failure_stop`，速度 0，`emergency_stop=true`。

停车点成功用例：

- 当前包 `semantics/parking_points.json` 中 `parking_points: []`。
- 因无真实 parking point ID，成功路径无法验证。
- 无效 parking point 失败路径已验证为安全停车。

## 3. 当前定位作为规划起点

验证命令：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

结果：

- 服务返回 `success=True`。
- 路线为 `N0001 -> N0002 -> N0003`。
- 轨迹为 30 点。
- 当前 pose 与轨迹起点距离为 0.0 m。
- status 中包含诊断信息：`matched current pose to waypoint=WP_E_C-001_F_000000 edge=E_C-001_F start_node=N0001 ...`。

## 4. 控制与 SCU 输出

SCU topic：

- topic：`/yunle_chassis/control/scu_control_command`
- type：`chassis_interfaces/msg/ScuControlCommand`
- publisher count：1
- subscriber count：0，本次没有真实 chassis subscriber，因此没有驱动真实车辆。
- 频率：约 50 Hz。

安全停车样本：

```text
scu_shift_level_request: 1
scu_steering_angle_front: 0.0
scu_steering_angle_rear: 0.0
scu_target_speed: 0.0
scu_brake_enable: true
```

前进路线输出：

- 规划成功后，SCU 短时间出现非零速度样本。
- 最大观测速度约 3.6 km/h。
- shift 为 1，即 D 档。
- speed 非负。
- 前轮转角字段单位为 deg，观测到约 `-5.23 deg`。
- 随后 control 进入 `trajectory_timeout`，SCU 回到安全停车。

倒车路线输出：

- 路线 `N0015 -> N0014` 成功，edge 为 `E_C-007_R`。
- trajectory gear 为 2。
- SCU 窗口中出现 shift 3，即 R 档。
- 倒车速度仍为非负 km/h。
- 未观察到非法 shift。

## 5. 安全验证

- 暂停仿真后空起点规划失败，错误信息为 `current localization pose is stale: age=... timeout=1s`。
- stale pose 失败时发布 `failure_stop`，速度 0。
- 手动发布 safety estop 后，control 状态进入 `safety_estop`。
- estop 下 SCU 全程 speed=0、brake=true、shift 合法。

## 6. 结论

仿真、路网节点规划、当前定位作为起点、SCU topic/type、D/R 档映射、安全停车和 estop 均具备运行阶段证据。

任务点规划、停车点成功规划和持续控制跟踪仍未满足进入真实车辆运动测试的要求。建议仅继续 bench-only 或 wheels-off 验证。
