# Ubuntu ROS2 仿真定位 path_follow 复测步骤

## 构建

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

## 启动 Roadnet A 仿真

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_1

ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  rviz:=true
```

另一个终端启动 planning/control：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
source install/setup.bash

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET
```

## 确认初始 pose

```bash
ros2 topic echo --once /localization/pose
ros2 topic echo /simulation/status
ros2 topic list | grep simulation
```

期望：

- `/localization/pose` 持续发布。
- `/simulation/status` 初始为 `waiting_for_path` 或 `active`。
- `/simulation/pose_path` 存在。

## 下发 task 目标

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-001'}"
```

观察：

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

- `/localization/pose` 连续变化。
- `/simulation/status` 为 `following_path`，message 显示 `source=full_reference_path`。
- 同一条 `/planning/full_reference_path` 10 Hz 重发不会让 pose 跳回起点。
- `/control/status` 保持 tracking。

## 下发 parking 目标

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'parking', goal_id: 'RP-015'}"
```

期望：

- 仿真 pose 从当前 pose 在新路径最近点 reanchor 后继续移动。
- 到达后 `/simulation/status` 为 `arrived`。

## 下发 charging 目标

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'charging', goal_id: 'RP-017'}"
```

期望：

- `RP-017` 规划成功。
- 仿真 pose 沿 `/planning/full_reference_path` 移动。
- 到达后保持终点 pose。

## pause/start/reset

```bash
ros2 service call /simulation/pause std_srvs/srv/Trigger "{}"
ros2 topic echo --once /localization/pose
sleep 2
ros2 topic echo --once /localization/pose
ros2 service call /simulation/start std_srvs/srv/Trigger "{}"
ros2 service call /simulation/reset std_srvs/srv/Trigger "{}"
```

期望：

- pause 后 pose 不再沿路径前进，但仍发布。
- start 后继续移动。
- reset 后回到配置初始 pose。

## failure_stop

调用一个无效目标：

```bash
ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'BAD_TASK'}"

ros2 topic echo --once /simulation/status
ros2 topic echo --once /localization/pose
```

期望：

- `/simulation/status` 为 `holding_failure_stop`。
- `/localization/pose` 保持当前位置。
- control 仍输出安全停车。

## Roadnet B 回归

```bash
ROADNET=/absolute/path/to/roadnet_ad_package_20260610T012525Z_2

ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=$ROADNET \
  use_sim_pose:=true \
  pose_mode:=path_follow \
  rviz:=true

ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=$ROADNET

ros2 service call /low_speed_av_planning/plan_mission \
  low_speed_av_interfaces/srv/PlanMission \
  "{start_type: 'current_pose', start_id: '', goal_type: 'task', goal_id: 'RP-003'}"
```

期望：

- Roadnet B task 目标仍可规划。
- 仿真 pose 连续移动。

## 验收

- 启动后发布初始 `/localization/pose`。
- 下发 task/parking/charging 后 pose 沿路径实时移动。
- path republish 不重置 progress。
- 新路径从当前 pose reanchor。
- pause/start/reset 正常。
- failure_stop/invalid goal 保持当前位置。
- SCU 27 deg clamp 和 `/control/status` heartbeat 不回退。

