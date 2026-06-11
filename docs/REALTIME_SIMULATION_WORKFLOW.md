# 实时仿真规划循迹控制流程

## 端到端数据流

```text
roadnet_ad_package_20260610T012525Z_1 / _2
  -> roadnet_visualization_node
  -> RViz roadnet / semantic markers
  -> sim_localization_pose_publisher_node
  -> /localization/pose
  -> /low_speed_av_planning/plan_mission 或 /plan_route
  -> /planning/global_route
  -> /planning/full_reference_path
  -> /planning/trajectory
  -> sim_localization_pose_publisher_node path_follow
  -> /localization/pose 连续移动
  -> low_speed_av_control
  -> /yunle_chassis/control/scu_control_command
```

## 启动策略

仿真定位只在仿真 launch 中默认启用：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1 \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  rviz:=true
```

真实车辆模式不要启动 `use_sim_pose:=true`，避免仿真定位覆盖真实定位源。

Planning/control 单独启动：

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1
```

## 初始 Pose

启动后，在未规划前，仿真节点持续发布配置中的 `/localization/pose`。默认 explicit pose：

```yaml
x: 0.554
y: 1.473
yaw: -0.9178
```

可通过 launch 覆盖：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  pose_mode:=path_follow \
  initial_source:=edge_progress \
  initial_edge_id:=E_C-012_F \
  initial_edge_progress:=0.55
```

## 下发任务

推荐业务入口是 `/plan_mission`：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"
```

Roadnet A 可验证：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'parking', goal_id: 'RP-015'}"

ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

旧 `/plan_route` 仍保留用于 node-id 或兼容验证。

## 观察

```bash
ros2 topic echo /localization/pose
ros2 topic echo /simulation/status
ros2 topic echo /simulation/pose_path
ros2 topic echo /planning/full_reference_path
ros2 topic echo /planning/trajectory
ros2 topic echo /control/status
ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- 下发目标前，`/localization/pose` 为初始 pose。
- 下发目标后，`/localization/pose` 连续变化。
- `/simulation/status` 从 `waiting_for_path` 变为 `following_path`，到终点后为 `arrived`。
- `/control/status` 在路径和定位新鲜时保持 tracking。
- SCU command 持续输出，安全策略仍由 control 执行。

## pause/start/reset

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
ros2 service call /simulation/rewind_path std_srvs/srv/Trigger "{}"
```

- `pause`：停止移动，但继续发布当前 pose。
- `start`：继续移动。
- `reset`：回到配置初始 pose。
- `rewind_path`：将当前路径 progress 归零，主要用于调试。

## failure_stop

如果规划失败、`/planning/trajectory` 为 `failure_stop` 或 `emergency_stop=true`，仿真节点进入：

```text
holding_failure_stop
```

此时保持当前位置，不沿旧 full reference path 继续移动。

