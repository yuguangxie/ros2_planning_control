# 当前定位起点锚点设计

## 问题

当 `/localization/pose` 位于有向 edge 中段时，如果规划器只把当前位置匹配成 topology node，会丢失当前 edge 的剩余几何段。Roadnet A 的复测场景中，当前定位接近：

```text
waypoint: WP_E_C-012_F_000039
edge: E_C-012_F
from: N0003
to: N0004
s_on_edge: 7.8 m
```

旧行为会把起点折叠为 `N0004`，随后 full reference path / local trajectory 从后续 `E_C-002_F` 开始，导致第一轨迹点距当前定位约 6 m 以上。

## 新增参数

```yaml
planning:
  start_anchor:
    include_current_edge_prefix: true
    max_start_projection_distance_m: 2.0
    max_first_trajectory_point_distance_m: 2.0
```

含义：

| 参数 | 默认值 | 含义 |
|---|---:|---|
| `planning.start_anchor.include_current_edge_prefix` | `true` | 当前定位匹配到 edge 中段时，full reference path 先加入当前位置到 edge 出口节点的剩余段。 |
| `planning.start_anchor.max_start_projection_distance_m` | `2.0` | 当前定位到最近 waypoint/projection 的最大允许距离。 |
| `planning.start_anchor.max_first_trajectory_point_distance_m` | `2.0` | 控制用 local trajectory 第一轨迹点到当前定位的最大允许距离。 |

## 数据合同

当前定位解析成 `RoadnetAnchor` 后保留：

- `edge_id`
- `edge_from_node_id`
- `edge_to_node_id`
- `waypoint_index`
- `s_on_edge_m`
- `edge_progress`
- 当前 pose 的 `x/y/yaw`

拓扑规划仍以节点为边界运行。若当前定位在 edge 中段，拓扑路线从 `edge_to_node_id` 开始，但完整几何路径会先补上：

```text
current pose projection
  -> current edge remaining waypoints
  -> edge_to_node
  -> global route edge sequence
  -> semantic goal cropped segment
```

## 验收标准

Roadnet A `current_pose -> RP-001` 应满足：

- `/planning/full_reference_path` 开头包含 `E_C-012_F` 从 `WP_E_C-012_F_000039` 附近到 `N0004` 的剩余段。
- `/planning/trajectory` 第一轨迹点距当前定位/projection 小于 `2.0 m`。
- 控制用 `/planning/trajectory` 不出现几何跳变。

Roadnet A `current_pose -> RP-008` 应由倒车策略决定：

- 倒车禁用：不输出 reverse gear，尝试 forward detour 或清晰失败。
- 倒车启用：允许同 edge reverse local segment，并在 status/message 中明确说明。

