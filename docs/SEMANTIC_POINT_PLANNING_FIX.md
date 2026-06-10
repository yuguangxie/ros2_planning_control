# Semantic Point Planning Fix

## 目标

修复 Ubuntu ROS2 验证中真实任务点规划失败的问题。现象是使用 `goal_task_point_id: RP-001` 或 `start_task_point_id: RP-003` 时，服务返回：

```text
start or goal node is not in topology
```

根因是语义点字段中存在 JSON/YAML `null`，例如：

```json
"linked_node_id": null,
"linked_edge_id": "E_L-011_F"
```

旧逻辑可能把 `linked_node_id: null` 当成非空字符串处理，导致没有 fallback 到 `linked_edge_id`。

## 修复后的空值规则

以下值统一按空值处理：

- 字段缺失
- YAML/JSON `null`
- 空字符串 `""`
- 只有空白字符的字符串
- 字符串 `"null"`
- 字符串 `"none"`

适用字段包括：

- `id`
- `type`
- `linked_node_id`
- `linked_edge_id`
- `linked_path_id`

## 任务点目标解析规则

当 `PlanRoute` 使用：

```yaml
goal_task_point_id: "RP-001"
```

规划节点按以下顺序解析目标节点：

1. 如果 `linked_node_id` 非空，并且该 node 存在于 topology 中，使用该 node。
2. 如果 `linked_node_id` 为空或无效，但 `linked_edge_id` 有效，使用该 edge 的 `to_node_id`。
3. 如果两者都无效，失败并返回清晰错误：

```text
task point RP-001 has no valid linked_node_id or linked_edge_id
```

当前正式包中：

```text
RP-001 -> linked_edge_id=E_L-011_F
E_L-011_F: N0009 -> N0008
```

因此 `goal_task_point_id: RP-001` 会解析为目标节点：

```text
N0008
```

## 任务点起点解析规则

当 `PlanRoute` 使用：

```yaml
start_task_point_id: "RP-003"
```

当前实现仍是 node-level fallback，不做精确 edge projection：

1. 如果 `linked_node_id` 有效，使用该 node。
2. 如果 `linked_edge_id` 有效，使用该 edge 的 `from_node_id`。
3. 如果两者都无效，安全失败。

这意味着任务点起点会落到关联边的起点节点，而不是任务点在边上的精确投影点。后续如需更精确的任务点起步，可在规划模块中增加 edge projection 和轨迹裁剪。

## Parking / Charging Point

当前服务字段支持：

```yaml
goal_parking_point_id
```

停车点目标使用与任务点目标相同的规则：

- 有效 `linked_node_id` 优先。
- 否则 fallback 到 `linked_edge_id` 的 `to_node_id`。
- 无效时清晰失败并发布安全停车轨迹。

当前正式路网包：

```text
semantics/parking_points.json -> parking_points: []
semantics/charging_points.json -> charging_points: []
```

因此 Ubuntu 正式包上无法验证真实 parking point 成功路径。离线脚本会构造临时 parking point fixture，验证：

- valid parking point -> fallback 到 edge node
- invalid parking point -> 清晰失败

## 安全行为

任何语义点解析失败都会保持原有安全语义：

- `PlanRoute` 返回 `success=false`
- `/planning/status` 发布 failure
- `/planning/trajectory` 发布 `failure_stop`
- trajectory speed 为 0
- `emergency_stop=true`
- 控制模块最终会输出 SCU 安全停车命令

## 离线验证

Windows Codex 环境可运行：

```powershell
uv run --with pyyaml python scripts\offline_runtime_followup_smoke.py roadnet_ad_package_20260610T012525Z
```

期望输出包含：

```text
Runtime follow-up smoke OK
RP-001_goal=N0008
parking_fixture=ok
```

