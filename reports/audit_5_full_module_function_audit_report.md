# Audit 5 Full Module Function Audit Report

## Goal

完成当前 ROS2 低速自动驾驶项目的全模块功能审计，并输出人工 ROS2 验证流程。

## Audit folder

Created:

```text
docs/audits_5/
```

## Files created

- `docs/audits_5/AUDIT_5_SUMMARY.md`
- `docs/audits_5/INTERFACES_AUDIT.md`
- `docs/audits_5/ROADNET_AD_PACKAGE_AUDIT.md`
- `docs/audits_5/PLANNING_MODULE_FUNCTION_AUDIT.md`
- `docs/audits_5/CURRENT_POSE_START_PLANNING_AUDIT.md`
- `docs/audits_5/CONTROL_MODULE_FUNCTION_AUDIT.md`
- `docs/audits_5/SCU_OUTPUT_AND_SAFETY_AUDIT.md`
- `docs/audits_5/SIMULATION_VISUALIZATION_AUDIT.md`
- `docs/audits_5/LAUNCH_CONFIG_AND_PARAMETERS_AUDIT.md`
- `docs/audits_5/DATAFLOW_INTEGRATION_AUDIT.md`
- `docs/audits_5/TESTING_AND_OFFLINE_SMOKE_AUDIT.md`
- `docs/audits_5/ROS2_MANUAL_VALIDATION_PROCEDURE.md`
- `docs/audits_5/ROS2_MANUAL_VALIDATION_CHECKLIST.md`
- `docs/audits_5/RISK_REGISTER_5.md`
- `docs/audits_5/FIX_PLAN_5.md`
- `docs/audits_5/AUDIT_5_INDEX.md`

## Commands run

```powershell
git status --short
git diff --stat
rg -n "catkin|roscpp|scu_drive_mode_request|/yunle_chassis/control/scu_control_command|ScuControlCommand|PlanRoute|ReloadRoadnet|localization_pose_topic|MarkerArray|PoseStamped|roadnet_ad_package_20260610T012525Z" .
uv run python scripts\validate_expected_tree.py
uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package
uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py
uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py
uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z
```

## Results

```text
Expected tree OK: .
AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)
Offline algorithm smoke OK
Remaining fixes smoke OK
Offline SCU/LQR smoke OK
Offline simulation smoke OK
```

## SKIPPED_ROS2_UNAVAILABLE

Current environment did not provide `colcon` or `ros2`.

Skipped:

```bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
colcon test
colcon test-result --verbose
ros2 launch low_speed_av_simulation simulation_visualization.launch.py ...
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
ros2 service call /low_speed_av_planning/plan_route ...
ros2 topic echo /planning/trajectory
ros2 topic echo /yunle_chassis/control/scu_control_command
```

## Top findings

1. `AUD5-SUM-001` P1: ROS2 build/test/launch 未验证。
2. `AUD5-SIM-001` P1: 新 simulation 包未在 ROS2 编译验证。
3. `AUD5-SCU-004` P1: SCU 输出未在真实 chassis driver/bench 验证。
4. `AUD5-CP-003` P2: current-pose start matcher 是 waypoint 近邻启发式，复杂边界场景需要增强。
5. `AUD5-SIM-003` P2: RViz waypoint 基础显示可能存在跨 edge 视觉伪连线。

## Module status

| Module | Status |
|---|---|
| interfaces | Pass static, Not Verified ROS2 |
| roadnet package | Pass offline |
| planning | Partial |
| current-pose start | Partial |
| control | Pass static/offline, Not Verified ROS2 |
| SCU output/safety | Pass static/offline, Not Verified chassis |
| simulation visualization | Partial |
| launch/config | Partial |
| dataflow integration | Partial |
| offline tests | Pass |

## Next recommended action

在真实 ROS2 环境首先执行：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
```

若构建通过，再执行：

```bash
source install/setup.bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  launch_planning_control:=true \
  rviz:=true
```

