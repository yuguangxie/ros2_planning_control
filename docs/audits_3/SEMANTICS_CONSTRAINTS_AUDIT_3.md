# 语义约束审计 3

## Objective（目标）
审计 semantics areas、task_points、parking_points、charging_points 是否加载，以及 speed_zone、no_go、keepout 是否实际影响规划或轨迹速度。

## Status（状态）
Partial。语义文件已加载，speed_zone 会降低轨迹速度，no_go/keepout 会阻断相关边；但几何约束仍为保守简化版，样例包缺少显式 no_go/speed_zone 区域用于静态人工核查。

## Evidence（证据）
- semantic area 数据结构：`src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:80` 到 `src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:82`。
- task/parking/charging 计数结构：`src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:91` 到 `src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:94`。
- semantic polygon finite 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:405` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:406`。
- no_go/keepout blocked_edges：`src/low_speed_av_planning/src/roadnet_loader.cpp:434`。
- speed_zone 降速：`src/low_speed_av_planning/src/planning_node.cpp:252` 到 `src/low_speed_av_planning/src/planning_node.cpp:265`。
- 样例 areas 有 `drivable_area` 和 speed limit：`src/low_speed_av_bringup/sample_ad_package/semantics/areas.json:2` 到 `src/low_speed_av_bringup/sample_ad_package/semantics/areas.json:27`。
- remaining fixes smoke 包含语义 route/speed 检查：`scripts/offline_remaining_fixes_smoke.py:195` 到 `scripts/offline_remaining_fixes_smoke.py:200`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-SE-001 | P3 | Pass | semantics 不再只是加载，speed_zone/no_go 已进入规划决策。 |
| A3-SE-002 | P2 | Partial | no_go/keepout 判断基于参考点落入 polygon，未覆盖边段穿越和车辆 footprint 膨胀。 |
| A3-SE-003 | P3 | Partial | task/parking/charging 点已加载计数，但目标解析仍是 skeleton/TODO 级别。 |
| A3-SE-004 | P2 | Partial | sample AD Package 未包含显式 no_go_area 或 speed_zone 示例，依赖 smoke 脚本临时样例验证。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
speed_zone 可以降低速度，有助于低速安全。no_go/keepout 当前实现能阻断命中的边，但复杂几何场景可能漏检，影响禁行区安全性。

## Recommended fix（推荐修复）
- 增加 segment-polygon intersection 和 footprint dilation。
- 在 sample AD Package 增加一块非默认 no_go_area 和 speed_zone，并保持 validation/checksum 更新。
- 将 task/parking/charging target resolution 从 skeleton 扩展为服务请求可选目标解析。

## Verification method（验证方法）
- 已运行 `offline_remaining_fixes_smoke.py`，验证语义影响 route/speed。
- 静态确认 C++ 约束代码存在。
- 未运行 C++ 或 ROS2 语义规划测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 service call /low_speed_av_planning/plan_route ...`
- `ros2 topic echo /planning/global_route`
- `ros2 topic echo /planning/trajectory`
