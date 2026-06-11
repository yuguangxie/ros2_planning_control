# Reverse Policy And Start Anchor Fix Report

## 目标

根据 Ubuntu ROS2 复测结果，修复两个运行阶段问题：

1. 倒车规划必须由显式配置控制，默认不静默倒车。
2. 当前定位位于 edge 中段时，`/planning/full_reference_path` 和 `/planning/trajectory` 必须从当前位置/projection 附近开始，不能直接从下一个 topology node 后的 edge 开始。

## 修改文件

- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `scripts/offline_semantic_goal_followup_smoke.py`
- `scripts/offline_reverse_policy_smoke.py`
- `docs/START_EQUALS_GOAL_EDGE_CASE_FIX.md`
- `reports/semantic_goal_planning_followup_fix_report.md`
- `docs/REVERSE_PLANNING_POLICY.md`
- `docs/CURRENT_POSE_START_ANCHOR_DESIGN.md`
- `docs/UBUNTU_ROS2_REVALIDATION_AFTER_REVERSE_POLICY_FIX.md`
- `reports/reverse_policy_and_start_anchor_fix_report.md`

未修改正式路网包：

- `roadnet_ad_package_20260610T012525Z_1`
- `roadnet_ad_package_20260610T012525Z_2`

## 关键设计

### 倒车策略

新增参数：

```yaml
planning:
  reverse:
    allow_reverse_planning: false
    allow_reverse_local_segment: false
    require_reverse_confirmation: true
    prefer_forward_route_when_reverse_disabled: true
    fail_if_goal_behind_on_same_edge_when_reverse_disabled: false
```

`read_global_options()` 现在将 `global_planner.allow_reverse` 与 `planning.reverse.allow_reverse_planning` 同时作为条件。默认情况下，即使旧配置中 `global_planner.allow_reverse=true`，全局 reverse edge 也不会被使用。

同 edge 后方语义目标：

- 倒车禁用：先尝试 forward detour；不可达则失败并发布 `failure_stop`。
- 倒车启用且局部倒车启用：生成 reverse local segment，并在 response/status 中写入 `reverse local segment selected`。

### 当前定位起点锚点

新增参数：

```yaml
planning:
  start_anchor:
    include_current_edge_prefix: true
    max_start_projection_distance_m: 2.0
    max_first_trajectory_point_distance_m: 2.0
```

当前定位匹配到 edge 中段后，`RoadnetAnchor` 保留 edge、waypoint index、`s_on_edge` 和当前 pose。拓扑路线仍从当前 edge 的出口节点开始，但 full reference path 会先加入当前位置到出口节点的剩余段。

## 已覆盖的 Ubuntu 问题

- Roadnet A `current_pose -> RP-001`：修复 first trajectory point 距当前定位过远的问题，轨迹应从 `E_C-012_F` 当前 projection 附近开始。
- Roadnet A `current_pose -> RP-008`：倒车行为改为配置控制。默认不输出 reverse gear；开启策略后才允许 reverse local segment。
- 已通过能力保持：charging `RP-017`、parking `RP-015`、Roadnet B `RP-003`、trajectory republish、SCU 27 deg clamp、control status heartbeat。

## 离线检查

已运行：

```text
uv run python scripts/offline_reverse_policy_smoke.py
```

结果：

```text
offline_reverse_policy_smoke: PASS
```

该脚本使用 `roadnet_ad_package_20260610T012525Z_1/trajectory/waypoints.csv` 和 JSON 文件，不依赖 ROS2 或 PyYAML。覆盖：

- Roadnet A `current_pose(E_C-012_F / WP_E_C-012_F_000039) -> RP-001`
- Roadnet A `current_pose -> RP-008`，reverse disabled
- Roadnet A `current_pose -> RP-008`，reverse enabled
- Roadnet B `current_pose -> RP-003`
- 配置默认值检查

同时运行了以下离线检查：

```text
uv run python scripts/offline_scu_lqr_smoke.py
uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_1
uv run --with pyyaml python scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_2
uv run --with pyyaml python scripts/offline_trajectory_continuity_smoke.py
uv run --with pyyaml python scripts/offline_semantic_goal_followup_smoke.py
uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_1
uv run --with pyyaml python scripts/offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_2
uv run --with pyyaml python scripts/offline_algorithm_smoke.py src/low_speed_av_bringup/sample_ad_package
uv run --with pyyaml python scripts/offline_remaining_fixes_smoke.py
```

结果均通过。首次不带 `--with pyyaml` 运行 YAML 类脚本时，当前 `uv run python` 环境缺少 PyYAML，报错为 `No module named 'yaml'` 或 `PyYAML required`；随后按脚本提示使用 `uv run --with pyyaml ...` 复跑通过。

## ROS2 命令

当前 Windows Codex 环境未执行 ROS2 命令：

```text
SKIPPED_ROS2_UNAVAILABLE
```

未声称以下命令通过：

- `colcon build --symlink-install`
- `colcon test`
- `ros2 launch`
- `ros2 service call`
- `ros2 topic echo`

## 已知限制

- `planning.reverse.require_reverse_confirmation` 当前仅作为策略表达和文档约束；现有 `PlanRoute/PlanMission` 接口没有倒车确认字段，因此通过 response/status message 明确提示 reverse selected。
- RViz marker 仍主要依赖现有 trajectory/status 内容区分 forward/reverse；未做大规模可视化样式重构。
- Windows 环境无法验证真实 ROS2 topic 频率、服务响应和 SCU shift；需要按 Ubuntu 复测文档执行。

## 下一步 Ubuntu 复测

按 `docs/UBUNTU_ROS2_REVALIDATION_AFTER_REVERSE_POLICY_FIX.md` 执行：

1. 构建和 `colcon test`。
2. Roadnet A `current_pose -> RP-001`，确认轨迹从当前 edge 中段附近开始。
3. Roadnet A `current_pose -> RP-008`，分别验证 reverse disabled/enabled。
4. 回归 Roadnet A `RP-017/RP-015`、Roadnet B `RP-003`、SCU 27 deg clamp、`/control/status` heartbeat。
