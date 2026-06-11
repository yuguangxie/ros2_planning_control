# Sim Localization Path Follow Fix Report

## 目标

完善实时仿真定位跟随能力：启动仿真后先发布初始 `/localization/pose`，规划成功后根据 `/planning/full_reference_path` 优先驱动仿真 pose 沿路径移动，fallback 到 `/planning/trajectory`，并避免规划周期重发导致进度重置。

## 修改文件

- `src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp`
- `src/low_speed_av_simulation/config/simulation_params.yaml`
- `src/low_speed_av_simulation/launch/simulation_visualization.launch.py`
- `src/low_speed_av_simulation/CMakeLists.txt`
- `scripts/offline_sim_localization_follow_smoke.py`
- `docs/SIM_LOCALIZATION_PATH_FOLLOW_DESIGN.md`
- `docs/REALTIME_SIMULATION_WORKFLOW.md`
- `docs/UBUNTU_ROS2_REVALIDATION_AFTER_SIM_PATH_FOLLOW.md`
- `reports/sim_localization_path_follow_fix_report.md`

未修改正式路网包：

- `roadnet_ad_package_20260610T012525Z_1`
- `roadnet_ad_package_20260610T012525Z_2`

## 设计决策

- 仿真定位仍由 `low_speed_av_simulation` 负责，planning 不发布定位。
- 新默认模式为 `path_follow`。
- 启动后无路径时持续发布初始 pose。
- 优先跟随 `/planning/full_reference_path`，因为它是完整连续几何路线。
- `/planning/trajectory` 作为 fallback，同时用于接收 `failure_stop / emergency_stop` 并保持当前位置。
- 使用路径内容签名去重，不依赖每次重发都会变化的 `trajectory_id`。
- 新路径默认从当前 pose 在路径上的最近点 reanchor，不跳到起点。
- 新增 `/simulation/status` 和 `/simulation/pose_path`。

## 新 topic / service

```text
/simulation/status
low_speed_av_interfaces/msg/ModuleStatus

/simulation/pose_path
nav_msgs/msg/Path

/simulation/rewind_path
std_srvs/srv/Trigger
```

保留：

```text
/simulation/start
/simulation/pause
/simulation/reset
```

## 离线检查

已运行：

```text
uv run python scripts/offline_sim_localization_follow_smoke.py
```

结果：

```text
offline_sim_localization_follow_smoke: PASS
```

覆盖：

- 初始 pose。
- full reference path 跟随。
- 同一路径 republish 不重置。
- 新路径 reanchor。
- failure_stop hold。
- 到达终点后保持。
- reverse 路径按几何移动。
- quaternion 有效。
- fallback 到 `/planning/trajectory`。

同时运行：

```text
uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_1
uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_2
uv run python scripts/offline_reverse_policy_smoke.py
uv run python scripts/offline_scu_lqr_smoke.py
uv run --with pyyaml python scripts/offline_trajectory_continuity_smoke.py
uv run --with pyyaml python scripts/offline_semantic_goal_followup_smoke.py
uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_1
uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_2
```

结果均通过。

## ROS2 命令

当前 Windows Codex 环境未执行 ROS2 命令：

```text
SKIPPED_ROS2_UNAVAILABLE
```

未声称以下命令通过：

- `colcon build --symlink-install`
- `colcon test`
- `ros2 launch`
- `ros2 topic echo`
- `ros2 service call`

## 已知限制

- 这是理想路径跟随仿真，不包含真实车辆动力学、轮胎/转向响应、控制延迟。
- 如果要验证控制器动力学，应新增基于 `/control/command` 或 SCU command 的车辆模型。
- Windows 环境没有执行 ROS2 topic/service 级验证，需按 Ubuntu 复测文档确认。

## 下一步

在 Ubuntu ROS2 Humble 环境按 `docs/UBUNTU_ROS2_REVALIDATION_AFTER_SIM_PATH_FOLLOW.md` 验证 Roadnet A 的 task/parking/charging 和 Roadnet B 的 task 回归。
