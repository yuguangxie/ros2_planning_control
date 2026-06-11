# PlanMission 错误文案合同

## 目标

`/low_speed_av_planning/plan_mission` 面向业务语义目标，因此响应文案必须能帮助操作者区分：

- start 解析失败
- goal 解析失败
- route 不可达
- trajectory 生成失败
- trajectory 连续性检查失败

current-pose 匹配信息只能作为上下文，不能覆盖真正失败原因。

## 支持的类型

`start_type` 支持：

- `""`
- `current_pose`
- `current`
- `current_position`
- `node`
- `node_id`
- `task`
- `task_point`
- `parking`
- `parking_point`
- `park`
- `charging`
- `charging_point`
- `charge`

`goal_type` 支持：

- `node`
- `node_id`
- `task`
- `task_point`
- `parking`
- `parking_point`
- `park`
- `charging`
- `charging_point`
- `charge`

## 成功文案

推荐格式：

```text
ok; start: matched current pose ...; goal: charging point RP-017 resolved to node=... edge=...
```

## 解析失败文案

goal 失败优先：

```text
goal resolution failed: charging point not found: BAD_CHARGING; start: matched current pose ...
```

start 失败优先：

```text
start resolution failed: current localization pose is stale: age=... timeout=...; goal: charging point RP-017 resolved ...
```

语义点存在但 edge 无效：

```text
goal resolution failed: charging point RP-017 has invalid linked_edge_id: BAD_EDGE
```

语义点存在但投影失败：

```text
goal resolution failed: failed to project charging point RP-017 onto edge E_C-017_F: ...
```

## Route 失败文案

```text
route planning failed: no valid global route; start: ...; goal: ...
```

## Trajectory 失败文案

```text
motion planner produced empty trajectory
```

或：

```text
local trajectory continuity check failed: trajectory point jump too large: ...
```

失败路径必须继续发布 `failure_stop`，下游控制模块收到后应输出 SCU brake stop。

## ROS2 命令

Windows Codex 环境未执行 ROS2 命令：

```text
SKIPPED_ROS2_UNAVAILABLE
```
