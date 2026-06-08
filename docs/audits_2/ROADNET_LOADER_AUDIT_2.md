# RoadnetLoader 第二轮审计

## Objective
审计 `RoadnetLoader` 当前实现的结构化解析、AD Package v1.1 文件解析、validation/checksum 行为、waypoint index 边界、节点/边引用和 semantics 加载。

## Status: Partial
结构化解析、validation 拒载、waypoint 字段和 index bounds 已明显改进；C++ runtime SHA-256 校验仍未完成；缺少 C++ 负样例测试。

## Evidence
- 入口文件检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:115` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:117`。
- schema 和 version 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:120` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:126`。
- manifest validation 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:148` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:152`。
- validation report 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:156` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:162`。
- edge 节点引用检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:223` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:226`。
- waypoint 有限值检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:245` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:246`。
- waypoint index bounds 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:269` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:280`。
- checksum warning-only 行为：`src/low_speed_av_planning/src/roadnet_loader.cpp:327` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:334`。

## Findings
### A2-RL-001：字符串搜索解析风险已修复
- Severity: P1
- Status: Pass
- Impact on planning/control/vehicle operation: JSON/YAML 字段重排和消息文本不会再轻易误导 validation 判断。
- Recommended fix: 用负样例测试锁定行为。
- Verification method: 构造字段顺序变化、warning 文本含 failed 的 validation report。

### A2-RL-002：failed validation 和 blocking_errors 拒载已实现
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 失败路网不会进入 active planning。
- Recommended fix: 增加报告级和 manifest 级负样例。
- Verification method: `validation.status=failed` 或 `summary.blocking_errors=1` 时 loader 抛异常。

### A2-RL-003：waypoint index 新旧格式和边界检查已实现
- Severity: P2
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 避免轨迹拼接访问越界或错误截断。
- Recommended fix: 增加 `end_index_exclusive`、legacy `end_index`、越界、反向范围四类测试。
- Verification method: C++ loader CLI 或单元测试。

### A2-RL-004：checksum 校验仍是 warning-only
- Severity: P1
- Status: Fail
- Impact on planning/control/vehicle operation: 文件被篡改时 ROS2 runtime 可能继续使用错误路网，影响路线和速度安全性。
- Recommended fix: 实现 C++ SHA-256；比较 `checksums.sha256` 和 `manifest.hashes`；允许配置 mismatch 为 fatal。
- Verification method: 篡改任意 canonical 文件并运行 loader。

### A2-RL-005：semantics 加载完成但约束使用未完成
- Severity: P2
- Status: Partial
- Impact on planning/control/vehicle operation: 可以解析 task/parking/charging/areas，但不会自动避让 no-go 或施加 speed-zone。
- Recommended fix: 在 RoadnetPackage 中添加语义类型枚举/约束转换，并接入 global/speed planner。
- Verification method: semantic area 改变后 route 或 target_speed 应变化。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon test --packages-select low_speed_av_planning`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /low_speed_av_planning/reload_roadnet ...`

