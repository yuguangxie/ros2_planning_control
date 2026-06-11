# Semantic Goal Planning Follow-up Fix Report

## Goal

根据 Ubuntu ROS2 复测结果，修复语义目标点规划、`route_N0001_N0001` 空轨迹、SCU 转角限值、`/control/status` 可观测性，并补充仿真复测支持。

## Files Changed

- `src/low_speed_av_interfaces/srv/PlanMission.srv`
- `src/low_speed_av_interfaces/CMakeLists.txt`
- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_control/include/low_speed_av_control/control_types.hpp`
- `src/low_speed_av_control/include/low_speed_av_control/control_node.hpp`
- `src/low_speed_av_control/src/control_node.cpp`
- `src/low_speed_av_control/src/scu_command_mapper.cpp`
- `src/low_speed_av_control/config/control_params.yaml`
- `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp`
- `src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp`
- `src/low_speed_av_simulation/config/simulation_params.yaml`
- `src/low_speed_av_simulation/launch/simulation_visualization.launch.py`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `scripts/offline_semantic_goal_followup_smoke.py`
- `docs/SEMANTIC_GOAL_PLANNING_DESIGN.md`
- `docs/PLANNING_OUTPUT_DATA_CONTRACT.md`
- `docs/START_EQUALS_GOAL_EDGE_CASE_FIX.md`
- `docs/SCU_STEERING_LIMIT_ALIGNMENT.md`
- `docs/UBUNTU_ROS2_REVALIDATION_AFTER_SEMANTIC_GOAL_FIX.md`

## Key Design Decisions

- 保留 `/plan_route` 和 `PlanRoute.srv` 兼容旧调用。
- 新增 `/plan_mission` 和 `PlanMission.srv`，用于 task/parking/charging 语义目标。
- planning 内部新增 `RoadnetAnchor`，保留语义点几何位置和 linked edge 投影。
- `start_node == goal_node` 时先做几何到达判断；未到达则生成局部裁剪 trajectory。
- 对当前 `N0001 -> RP-003` 场景，默认允许 reverse local segment。
- `/planning/trajectory` 仍为控制输入，默认 10 Hz 重发。
- SCU 出口默认最大转角改为 `27.0 deg`，超限默认 clamp。
- `/control/status` 增加默认 5 Hz tracking 心跳。

## AD Package Compatibility

正式 `roadnet_ad_package_20260610T012525Z` 未被修改。语义点读取仍基于：

- `semantics/task_points.json`
- `semantics/parking_points.json`
- `semantics/charging_points.json`
- `trajectory/waypoints.yaml`
- `trajectory/waypoint_index.json`
- `roadnet/topology.json`

parking/charging 成功路径需要带相应点的包或离线 fixture；当前正式包 parking/charging 为空。

## Topic and Config Compatibility

- `/planning/global_route`：拓扑 route，可视化/调试。
- `/planning/trajectory`：控制真正跟踪的数据，持续发布。
- `/yunle_chassis/control/scu_control_command`：SCU 底盘输出，安全合同不变。
- `/control/status`：新增 tracking 周期状态。

新增 planning 参数：

```yaml
planning:
  arrival_radius_m: 0.5
  arrival_heading_tolerance_rad: 0.35
  semantic_goal_use_edge_projection: true
  semantic_goal_allow_reverse_local_segment: true
  semantic_goal_crop_waypoints: true
  semantic_goal_min_segment_length_m: 0.2
```

新增/更新 control 参数：

```yaml
control:
  status_publish_rate_hz: 5.0
scu:
  max_steering_angle_deg: 27.0
  overrange_policy: "clamp"
```

## Offline Checks

Windows Codex 环境的 `python` 是 WindowsApps 占位程序，`py` 不存在；本轮使用 `uv run --with pyyaml python ...` 执行离线脚本。

已执行：

```powershell
git status --short
git diff --stat
rg -n "RoadnetAnchor|task_point|parking_point|charging_point|linked_edge_id|linked_node_id|route_N0001_N0001|arrival_radius|edge_projection|crop|trajectory_republish|scu_max_steering|control/status|status_publish_rate" src docs scripts
uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package
uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py
uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py
uv run --with pyyaml python scripts\offline_runtime_followup_smoke.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_semantic_goal_followup_smoke.py roadnet_ad_package_20260610T012525Z
```

结果：

- `validate_sample_ad_package.py`：PASS，16 nodes / 22 edges / 496 waypoints。
- `offline_simulation_smoke.py`：PASS，仿真 marker、pose、route/trajectory 基础检查通过。
- `offline_algorithm_smoke.py`：PASS，node-id route、trajectory、Pure Pursuit/Stanley、Ackermann、estop 检查通过。
- `offline_remaining_fixes_smoke.py`：PASS，checksum、bad validation、bad index、semantics、LQR/MPC、estop 策略检查通过。
- `offline_scu_lqr_smoke.py`：PASS，SCU mapping、Ackermann、Riccati LQR 检查通过。
- `offline_runtime_followup_smoke.py`：PASS，task point fallback、parking fixture、trajectory republish 静态检查通过。
- `offline_semantic_goal_followup_smoke.py`：PASS，`route_N0001_N0001` 场景、RP-003 reverse local segment、parking fixture、SCU 27 deg clamp、状态/重发配置检查通过。

ROS2 命令未在 Windows 环境执行。

## SKIPPED_ROS2_UNAVAILABLE

以下命令需要 Ubuntu ROS2 环境复测：

```bash
colcon build --symlink-install
colcon test
ros2 launch low_speed_av_simulation simulation_visualization.launch.py
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
ros2 service call /low_speed_av_planning/plan_route ...
ros2 service call /low_speed_av_planning/plan_mission ...
ros2 topic echo /planning/trajectory
ros2 topic echo /control/status
ros2 topic echo /yunle_chassis/control/scu_control_command
```

## Known Limitations

- 未在本 Windows 环境实际构建 ROS2。
- charging 目标通过新 `PlanMission.srv` 支持，旧 `PlanRoute.srv` 没有 charging 字段。
- 反向局部 trajectory 的实车舒适性仍需低速 bench/wheels-off 复测。
- 当前 roadnet 仍有高曲率和曲率连续性 warning，实车前应低速、限速并考虑重新导出或平滑。

## Next Handoff

在 Ubuntu ROS2 Humble 环境按 `docs/UBUNTU_ROS2_REVALIDATION_AFTER_SEMANTIC_GOAL_FIX.md` 执行复测，重点确认：

- `current pose -> RP-003` 成功或 arrived stop。
- `/planning/trajectory` 非空且持续发布。
- SCU steering 不超过 27 deg。
- `/control/status` active/tracking 可观测。
