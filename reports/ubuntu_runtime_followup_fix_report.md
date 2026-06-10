# Ubuntu Runtime Follow-Up Fix Report

## Goal

根据 Ubuntu ROS2 运行验证报告修复运行阶段问题，重点覆盖：

- 真实 task point 规划失败
- 规划轨迹只发布一次导致 control `trajectory_timeout`
- `/planning/roadnet_status` 晚订阅容易错过
- parking point 成功路径缺少正式包数据
- roadnet 曲率 warning 的安全建议

## Files Changed

- `src/low_speed_av_planning/src/roadnet_loader.cpp`
- `src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp`
- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `scripts/validate_expected_tree.py`
- `scripts/offline_runtime_followup_smoke.py`
- `docs/SEMANTIC_POINT_PLANNING_FIX.md`
- `docs/TRAJECTORY_LIFECYCLE_AND_REPUBLISH_DESIGN.md`
- `docs/UBUNTU_ROS2_REVALIDATION_AFTER_FIX.md`
- `reports/ubuntu_runtime_followup_fix_report.md`

正式路网包 `roadnet_ad_package_20260610T012525Z` 未修改。

## Ubuntu Issues Addressed

| ID | Issue | Fix |
|---|---|---|
| UVR-P1-001 | 真实 task point 规划失败，`linked_node_id: null` 疑似被解析成 `"null"` | RoadnetLoader 清洗 null/空字符串；PlanningNode 验证 linked node 是否真实存在，必要时 fallback 到 linked edge |
| UVR-P1-002 | planning service 只发布一次 trajectory，control 0.5 秒后 `trajectory_timeout` | PlanningNode 缓存最近一次成功 route/trajectory，并按参数周期重发 trajectory |
| UVR-P3-002 | `/planning/roadnet_status` late echo 容易错过 | roadnet status publisher 使用 transient local QoS，并支持周期重发 |
| UVR-P2-001 | 当前正式包没有 parking point，成功路径无法验证 | 新增离线 fixture 验证 parking point fallback；文档说明正式包仍需带 parking point 的包复测 |
| UVR-P2-002 | roadnet 有高曲率/曲率连续性 warning | 文档补充低速、曲率限速、平滑或重新导出建议 |

## Semantic Point Null / Fallback Rules

以下值统一按空处理：

- 缺失字段
- YAML/JSON `null`
- `""`
- 空白字符串
- `"null"`
- `"none"`

解析规则：

- 显式 `goal_node_id` / `start_node_id` 不变。
- `goal_task_point_id`：
  - 有效 `linked_node_id` 优先。
  - 否则使用有效 `linked_edge_id` 的 `to_node_id`。
- `start_task_point_id`：
  - 有效 `linked_node_id` 优先。
  - 否则使用有效 `linked_edge_id` 的 `from_node_id`。
- `goal_parking_point_id`：
  - 有效 `linked_node_id` 优先。
  - 否则使用有效 `linked_edge_id` 的 `to_node_id`。
- 无效语义点返回清晰错误，并发布安全停车轨迹。

当前实现仍是 node-level fallback，不是任务点在 edge 上的精确 projection/crop。

## Trajectory Republish Rules

新增参数：

```yaml
planning:
  republish_last_route: true
  republish_last_trajectory: true
  route_republish_rate_hz: 1.0
  trajectory_republish_rate_hz: 10.0
  roadnet_status_publish_rate_hz: 1.0
```

成功规划后：

- 保存最近一次成功 `GlobalRoute`。
- 保存最近一次成功 `Trajectory`。
- `/planning/trajectory` 默认 10 Hz 周期重发。
- `/planning/global_route` 默认 1 Hz 周期重发。

失败规划后：

- 清除旧运动 route。
- 发布并缓存 `failure_stop` trajectory。
- 不再继续重发旧的运动 trajectory。

控制 timeout 语义保持不变：如果 planning 不再发布 trajectory，control 仍可进入 `trajectory_timeout` 安全停车。

## Roadnet Status Rules

`/planning/roadnet_status` publisher 改为：

```text
reliable + transient local + depth 1
```

同时按 `planning.roadnet_status_publish_rate_hz` 周期重发最近状态。reload 后仍立即发布一次状态。

## Parking Point Verification Status

当前正式包：

```text
semantics/parking_points.json -> parking_points: []
```

因此 Ubuntu 正式包只能验证 parking point 失败路径。成功路径需要：

- 带真实 parking point 的 AD Package；或
- 离线 fixture / 临时测试包。

本轮新增脚本 `scripts/offline_runtime_followup_smoke.py` 使用临时 parking point fixture 验证 fallback 逻辑，不修改正式包。

## Roadnet Curvature Warning Safety Notes

当前包仍包含非阻塞 warning：

- `HIGH_CURVATURE`
- `CURVATURE_CONTINUITY`
- `WAYPOINT_CURVATURE_EXCEEDS_CONSTRAINT`

本轮不自动修改正式路网。实车前建议：

- 保持低速验证。
- 使用 `curvature` speed planner。
- 检查曲率限速效果。
- 必要时在路网编辑器中平滑并重新导出。

## Offline Checks Run In Windows Codex

已执行：

```powershell
uv run --with pyyaml python scripts\offline_runtime_followup_smoke.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\validate_expected_tree.py
uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z
uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package
uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py
uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py
```

结果：

```text
Runtime follow-up smoke OK: task_points=7, RP-001_goal=N0008, RP-003_to_RP-001_edges=['E_L-010_R', 'E_L-008_R', 'E_L-006_F', 'E_C-004_F'], parking_fixture=ok, republish_static=ok
Expected tree OK: .
AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)
Offline simulation smoke OK: nodes=16, edges=22, waypoints=496, areas=4, task_points=7, ...
Offline algorithm smoke OK: route=['E_L001_F', 'E_L002_F'], ...
Remaining fixes smoke OK: checksum/bad_validation/bad_index rejected, ...
Offline SCU/LQR smoke OK: SCU mapping safe, Ackermann finite, Riccati LQR finite/config-sensitive
```

裸 `python --version` 在当前 Windows Codex 环境中返回失败且无输出；本轮使用已知可用的 `uv` Python 执行离线脚本。

## ROS2 Commands Skipped

当前 Windows Codex 环境未检测到 ROS2/colcon，因此未执行：

```text
SKIPPED_ROS2_UNAVAILABLE: colcon build --symlink-install
SKIPPED_ROS2_UNAVAILABLE: colcon test
SKIPPED_ROS2_UNAVAILABLE: ros2 launch
SKIPPED_ROS2_UNAVAILABLE: ros2 service call
SKIPPED_ROS2_UNAVAILABLE: ros2 topic hz/echo
```

不声明 ROS2 build/launch/test 已通过。

## Next Ubuntu Revalidation

请按 `docs/UBUNTU_ROS2_REVALIDATION_AFTER_FIX.md` 执行。

优先复测：

1. `colcon build --symlink-install`
2. `goal_task_point_id: RP-001`
3. `start_task_point_id: RP-003` 到 `goal_task_point_id: RP-001`
4. `/planning/trajectory` 是否默认约 10 Hz
5. control 是否不再因单次 trajectory 发布而 0.5 秒后 timeout
6. `/planning/roadnet_status --once` 晚订阅是否能看到 ready
