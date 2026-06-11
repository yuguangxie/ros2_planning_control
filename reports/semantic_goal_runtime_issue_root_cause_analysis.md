# 语义目标运行问题根因分析

## 目标

根据 Ubuntu ROS2 语义目标复测结果，分析 Roadnet A 中 `/plan_mission current_pose -> charging RP-017` 失败、长距离语义目标 `/planning/trajectory` 不连续、PlanMission 错误文案不清晰、SCU 参数命名文档不一致、自动化测试不足等问题。

## 结论摘要

- P1：`RP-017` charging 失败的直接原因是正式路网包使用 `entry_edge_id` / `properties.linked_path_id` 表达充电点关联，而旧解析只主要依赖 `linked_edge_id`，导致 charging anchor 不能稳定投影到边。
- P1：长路线语义目标的控制轨迹存在不连续风险，根因是先生成 15 m horizon 局部 trajectory，再追加远处目标 edge 局部段。
- P2：PlanMission 失败文案优先显示 current-pose 匹配摘要，覆盖了真正的 goal 解析失败原因。
- P3：部分旧检查清单使用 `output.scu_*`，实际 canonical 参数为 `scu.max_steering_angle_deg` 和 `scu.overrange_policy`。
- P3：此前 `colcon test-result` 为 0 tests，缺少对语义目标和轨迹连续性的自动化回归入口。

## Charging RP-017 失败根因

Roadnet A 文件 `roadnet_ad_package_20260610T012525Z_1/semantics/charging_points.json` 中，`RP-017` 的关键字段为：

```text
id: RP-017
entry_edge_id: E_C-017_F
exit_edge_id: E_C-017_F
properties.linked_path_id: C-017
properties.s_on_path: 1.567...
pose.x/y/yaw: charging 几何位姿
```

它没有直接提供 `linked_edge_id`。旧逻辑对 task/parking 场景基本够用，但 charging 场景没有进入同一套 edge fallback 与 projection 规则。

修复点：

- `src/low_speed_av_planning/src/roadnet_loader.cpp:188` 的 `parse_semantic_point()` 现在会按顺序读取 `linked_edge_id`、`entry_edge_id`、`approach.edge_id`。
- 同一函数会读取 `linked_path_id`，并 fallback 到 `properties.linked_path_id`、`properties.path_id`。
- `properties.s_on_path` 会作为 `linked_s_m` 的 fallback。
- `src/low_speed_av_planning/src/planning_node.cpp:1377` 的 PlanMission 分支支持 `charging`、`charging_point`、`charge`。

因此 charging point 会和 task/parking point 一样解析为 RoadnetAnchor，包含 point id、edge id、from/to node、投影 waypoint、语义点几何位姿和 final stop 语义。

## 失败文案被覆盖根因

旧 PlanMission 在 start 和 goal 任意一侧失败时，优先返回 `start_diagnostic`。当 current pose 可以匹配成功，但 goal 是 `BAD_CHARGING` 时，响应可能只看到 current pose matched 摘要，操作者无法定位真正输入错误。

修复点：

- `src/low_speed_av_planning/src/planning_node.cpp:105` 新增统一的 resolution failure message 组合规则。
- goal 解析失败优先显示 goal 原因，并把 start 匹配信息作为上下文：

```text
goal resolution failed: charging point not found: BAD_CHARGING; start: matched current pose ...
```

## Trajectory 不连续根因

旧错误模式：

```text
global route 很长
  -> ReferenceLineMotionPlanner 根据 horizon_distance_m=15.0 生成局部前视段
  -> semantic goal 逻辑 append 远处目标 edge cropped segment
  -> /planning/trajectory = 近处前视段 + 远处目标段
```

这会让控制模块直接订阅的 `/planning/trajectory` 出现几何跳变。控制模块只消费 `/planning/trajectory`，不消费 `/planning/global_route`，因此这个 topic 必须连续、局部、可跟踪。

修复设计：

- `src/low_speed_av_planning/src/planning_node.cpp:495` 新增/使用完整参考路径构造：先根据完整拓扑 route stitch 全量 waypoint，再拼接语义目标 edge 的连续 cropped segment。
- `src/low_speed_av_planning/src/planning_node.cpp:546` 在完整参考路径完成后，再按当前定位和 `motion_planner.horizon_distance_m` 裁剪控制用局部 trajectory。
- `src/low_speed_av_planning/src/planning_node.cpp:577` 增加相邻点最大跳变检查，默认 `planning.max_trajectory_point_jump_m=2.0`。
- 新增 `/planning/full_reference_path`，用于完整几何路线可视化和诊断；不再把远处语义目标段拼进控制用局部 trajectory。

## SCU 参数命名根因

运行验证确认实际参数为：

```text
scu.max_steering_angle_deg=27.0
scu.overrange_policy=clamp
```

旧报告中的 `output.scu_max_steering_angle_deg` 是检查清单命名错误。`output.mode=scu_control_command` 仍保留，用于选择输出模式；SCU 物理限制归属 `scu.*`。

## 自动化测试风险

复测报告指出此前测试结果为 0 tests。此次新增：

- `scripts/offline_trajectory_continuity_smoke.py`
- `src/low_speed_av_planning/test/test_offline_trajectory_continuity.py`
- `src/low_speed_av_planning/CMakeLists.txt` 接入 `ament_cmake_pytest`

Windows Codex 环境没有 ROS2，因此 `colcon test` 未执行；Ubuntu 侧可通过 `colcon test` 纳入这个离线连续性 smoke。

## ROS2 命令

当前 Windows Codex 环境未执行 ROS2 命令：

```text
SKIPPED_ROS2_UNAVAILABLE: colcon build/test, ros2 launch/topic/service
```
