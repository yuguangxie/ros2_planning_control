# Charging 语义目标与轨迹连续性修复报告

## 修改目标

修复 Ubuntu ROS2 语义目标复测发现的问题：

1. Roadnet A `/plan_mission current_pose -> charging RP-017` 失败。
2. 长距离语义目标 `/planning/trajectory` 可能出现前视段与远处目标段跳接。
3. PlanMission 错误文案不清晰。
4. SCU 参数命名文档与实际参数不一致。
5. 自动化测试不足。

## 主要修改文件

- `src/low_speed_av_planning/src/roadnet_loader.cpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp`
- `src/low_speed_av_simulation/config/simulation_params.yaml`
- `scripts/offline_trajectory_continuity_smoke.py`
- `scripts/offline_semantic_goal_followup_smoke.py`
- `scripts/offline_simulation_smoke.py`
- `src/low_speed_av_planning/test/test_offline_trajectory_continuity.py`
- `src/low_speed_av_planning/CMakeLists.txt`
- `src/low_speed_av_planning/package.xml`
- `docs/TRAJECTORY_CONTINUITY_AND_FULL_REFERENCE_PATH_DESIGN.md`
- `docs/PLAN_MISSION_ERROR_MESSAGE_CONTRACT.md`
- `docs/PLANNING_OUTPUT_DATA_CONTRACT.md`
- `docs/SCU_STEERING_LIMIT_ALIGNMENT.md`
- `docs/UBUNTU_ROS2_REVALIDATION_AFTER_TRAJECTORY_CONTINUITY_FIX.md`

## Charging 修复

`RoadnetLoader` 的 semantic point 解析现在支持：

- `linked_edge_id`
- `entry_edge_id`
- `approach.edge_id`
- `properties.linked_path_id`
- `properties.path_id`
- `properties.s_on_path`

`PlanMission` 支持以下 charging 类型写法：

- `charging`
- `charging_point`
- `charge`

失败文案示例：

```text
goal resolution failed: charging point not found: BAD_CHARGING; start: matched current pose ...
```

## Trajectory 连续性修复

新合同：

- `/planning/global_route`：完整拓扑路线。
- `/planning/full_reference_path`：完整连续几何参考路线。
- `/planning/trajectory`：控制用连续局部轨迹。

规划节点先构造 full reference path，再按当前 pose 和 horizon 裁剪 `/planning/trajectory`。不再把远处语义目标 edge 直接 append 到局部控制轨迹尾部。

新增参数：

```yaml
planning:
  publish_full_reference_path: true
  full_reference_path_topic: "/planning/full_reference_path"
  local_trajectory_from_current_pose: true
  max_trajectory_point_jump_m: 2.0
```

## 仿真可视化

`roadnet_visualization_node` 新增订阅：

```text
/planning/full_reference_path
```

RViz 可区分：

- global route
- full reference path
- local trajectory
- current vehicle pose
- task/parking/charging labels

## SCU 参数命名

canonical 参数：

```text
scu.max_steering_angle_deg
scu.overrange_policy
```

`output.mode=scu_control_command` 仍保留。不要使用 `output.scu_*` 作为验证参数。

## 离线检查结果

在 Windows Codex 环境执行：

```text
uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_1
PASS: AD Package OK

uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_2
PASS: AD Package OK

uv run --with pyyaml python scripts/offline_semantic_goal_followup_smoke.py
PASS

uv run --with pyyaml python scripts/offline_trajectory_continuity_smoke.py
PASS

uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_1
PASS

uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_2
PASS

uv run --with pyyaml python scripts/offline_algorithm_smoke.py src/low_speed_av_bringup/sample_ad_package
PASS

uv run --with pyyaml python scripts/offline_remaining_fixes_smoke.py
PASS

uv run --with pyyaml python scripts/offline_scu_lqr_smoke.py
PASS

uv run --with pyyaml python -m py_compile scripts/offline_trajectory_continuity_smoke.py scripts/offline_semantic_goal_followup_smoke.py scripts/offline_simulation_smoke.py src/low_speed_av_planning/test/test_offline_trajectory_continuity.py
PASS

uv run --with pytest --with pyyaml python -m pytest src/low_speed_av_planning/test/test_offline_trajectory_continuity.py -q
PASS: 1 passed
```

## ROS2 命令

当前 Windows Codex 环境未执行：

```text
SKIPPED_ROS2_UNAVAILABLE: colcon build --symlink-install
SKIPPED_ROS2_UNAVAILABLE: colcon test
SKIPPED_ROS2_UNAVAILABLE: ros2 launch/topic/service
```

## 剩余风险

- 需要 Ubuntu ROS2 环境验证 `colcon build/test` 中新增 pytest 是否正常进入测试结果。
- 需要实测 `/planning/full_reference_path` 在 RViz 中与 `/planning/trajectory` 的区分显示。
- Roadnet A charging `RP-017` 如果仍失败，应根据新 message 判断是 graph 不可达、edge 投影失败还是数据本身不可用。
- 控制器仍需在安全台架验证 SCU 输出与底盘 driver 实际协议一致。

## 下一步

按 `docs/UBUNTU_ROS2_REVALIDATION_AFTER_TRAJECTORY_CONTINUITY_FIX.md` 执行 Ubuntu 复测，并重点检查：

- `/plan_mission current_pose -> charging RP-017`
- `/planning/full_reference_path`
- `/planning/trajectory` 连续性与 10 Hz
- invalid charging message
- SCU 27 deg clamp
- `/control/status` 5 Hz
