# RoadnetLoader 审计 3

## Objective（目标）
审计 RoadnetLoader 对 manifest、validation、checksum、topology、waypoints、waypoint_index、semantics 的读取与拒绝策略。

## Status（状态）
Partial。源码层面具备结构化读取、校验失败拒绝、blocking_errors 拒绝、SHA-256 mismatch 拒绝、索引边界检查和语义加载；但没有已编译执行的 C++ loader 测试结果。

## Evidence（证据）
- SHA-256 函数：`src/low_speed_av_planning/src/roadnet_loader.cpp:36`。
- manifest 读取和 schema 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:213` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:224`。
- manifest hashes 解析：`src/low_speed_av_planning/src/roadnet_loader.cpp:242`。
- manifest validation 拒绝：`src/low_speed_av_planning/src/roadnet_loader.cpp:253` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:255`。
- validation report 拒绝：`src/low_speed_av_planning/src/roadnet_loader.cpp:259` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:265`。
- waypoint finite 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:348` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:349`。
- waypoint index 边界检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:380` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:383`。
- semantic polygon finite 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:405` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:406`。
- no_go/keepout blocked_edges：`src/low_speed_av_planning/src/roadnet_loader.cpp:434`。
- checksum 解析和 mismatch 拒绝：`src/low_speed_av_planning/src/roadnet_loader.cpp:454` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:494`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-RL-001 | P3 | Pass | loader 不再把 checksum mismatch 当作 warning-only，`verify_checksums=true` 时 mismatch 抛异常。 |
| A3-RL-002 | P3 | Pass | failed validation 和 `blocking_errors > 0` 均会拒绝加载。 |
| A3-RL-003 | P3 | Pass | waypoint finite、必需字段和索引边界检查已实现。 |
| A3-RL-004 | P2 | Partial | semantics 加载和 blocked_edges 已实现，但 no_go 判断依赖参考点落入多边形，尚未覆盖边段穿越多边形但端点未落入的情况。 |
| A3-RL-005 | P2 | Not Verified | C++ checksum 实现未通过编译运行测试，只通过源码审计和 Python smoke 间接验证。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
checksum、validation、index 拒绝策略降低了损坏或失败地图进入规划链路的风险。语义几何检测不完整可能导致某些穿越禁行区的边未被阻断。

## Recommended fix（推荐修复）
- 为 C++ loader 添加 gtest，直接篡改 `trajectory/waypoints.yaml` 并断言加载失败。
- 将 no_go/keepout 从点包含检测升级为 segment-polygon intersection，并考虑车辆 footprint。
- 将 checksum warning/missing 策略参数写入 README 和集成计划。

## Verification method（验证方法）
- 已运行 Python 样例校验和 remaining fixes smoke。
- 静态确认 C++ mismatch throw 存在。
- 未运行 C++ loader 测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon test --packages-select low_speed_av_planning`
- `ros2 launch low_speed_av_planning planning.launch.py`

