# 倒车规划策略说明

## 目标

本项目的业务目标建议使用 `task / parking / charging` 语义点。语义点可能位于当前车辆所在有向 edge 的后方，例如 Roadnet A 中 `current_pose -> RP-008`：当前定位在 `E_C-012_F` 中段，`RP-008` 位于同一 edge 的较小 `s_on_edge` 位置。此时是否允许倒车不能由规划器静默决定，必须由配置显式控制。

## 参数

规划节点新增以下参数：

```yaml
planning:
  reverse:
    allow_reverse_planning: false
    allow_reverse_local_segment: false
    require_reverse_confirmation: true
    prefer_forward_route_when_reverse_disabled: true
    fail_if_goal_behind_on_same_edge_when_reverse_disabled: false
```

含义：

| 参数 | 默认值 | 含义 |
|---|---:|---|
| `planning.reverse.allow_reverse_planning` | `false` | 是否允许全局规划使用 reverse edge。即使 `global_planner.allow_reverse=true`，该参数为 `false` 时 reverse edge 仍被禁用。 |
| `planning.reverse.allow_reverse_local_segment` | `false` | 是否允许同 edge 后方语义目标生成临时倒车局部段。 |
| `planning.reverse.require_reverse_confirmation` | `true` | 当前接口没有确认字段，因此倒车被选中时必须在 response/status message 中明确说明。 |
| `planning.reverse.prefer_forward_route_when_reverse_disabled` | `true` | 禁止倒车时，优先尝试有向拓扑绕行到目标 edge 入口。 |
| `planning.reverse.fail_if_goal_behind_on_same_edge_when_reverse_disabled` | `false` | 禁止倒车且同 edge 目标在后方时是否直接失败；默认先尝试 forward detour。 |

旧参数 `planning.semantic_goal_allow_reverse_local_segment` 保留为兼容项，但默认已改为 `false`。当前实际倒车策略以 `planning.reverse.*` 为准。

## 行为

当倒车禁用：

- `/planning/trajectory` 不应包含 reverse gear。
- 同 edge 后方目标不生成 `semantic_reverse_local`。
- 规划器先尝试从当前 edge 出口节点按有向拓扑绕行到目标 edge 的入口节点，再沿目标 edge 正向裁剪到语义点。
- 如果 forward detour 不可达，服务失败并发布 `failure_stop`，message 包含：

```text
reverse planning is disabled and forward route to goal is unavailable
```

当倒车启用：

- 只有 `allow_reverse_planning=true` 且 `allow_reverse_local_segment=true` 时，才允许同 edge 后方目标生成 reverse local segment。
- response/status message 必须包含：

```text
reverse local segment selected
```

## 与 SCU 输出的关系

规划只表达轨迹点的 `gear` 语义。控制模块继续负责将内部命令映射到底盘 SCU：

- forward/drive -> `scu_shift_level_request=1`
- reverse -> `scu_shift_level_request=3`
- safe stop -> 有效挡位、速度 0、制动 true

默认配置禁止静默倒车，因此常规仿真和 bench 验证不会因为语义点在后方而直接下发 R 档。

Phase 15 不扩大倒车能力。Semantic terminal helper 仅在既有 reverse policy 明确允许时构造 reverse segment；progress window 结合 reference heading，不能被解释为启用新的倒车规划或倒车控制合同。
