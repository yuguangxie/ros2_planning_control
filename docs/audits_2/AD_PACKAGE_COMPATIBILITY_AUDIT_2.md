# AD Package v1.1 兼容性第二轮审计

## Objective
审计当前工程是否仍然以 Low Speed Roadnet AD Package v1.1 规范路径为主合同，并检查 manifest、validation、waypoints、waypoint_index、semantics 和旧路径禁用情况。

## Status: Partial
规范路径和字段映射基本通过；validation 拒载逻辑已增强；semantics 已加载。未完全通过的原因是 C++ runtime checksum/hash 校验仍未实现摘要比对，且 semantics 尚未参与速度区/禁行区规划约束。

## Evidence
- `project_manifest.json` 是入口：`src/low_speed_av_planning/src/roadnet_loader.cpp:115` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:117`。
- schema version 1.1.x 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:124` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:126`。
- validation report 路径：`src/low_speed_av_planning/src/roadnet_loader.cpp:156`。
- `trajectory/waypoints.yaml` 路径：`src/low_speed_av_planning/src/roadnet_loader.cpp:166`。
- `kappa -> kappa_1pm`：`src/low_speed_av_planning/src/roadnet_loader.cpp:239`。
- `v_mps -> target_speed_mps`：`src/low_speed_av_planning/src/roadnet_loader.cpp:240`。
- `end_index_exclusive` 优先，legacy `end_index` inclusive fallback：`src/low_speed_av_planning/src/roadnet_loader.cpp:269` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:272`。
- semantics 容器：`src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:98` 至 `src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp:102`。
- semantics 加载：`src/low_speed_av_planning/src/roadnet_loader.cpp:287` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:312`。
- runtime 源码中旧路径没有作为主路径；`rg` 结果显示旧路径仅出现在 README 的禁止说明和 canonical 文件名片段中。

## Findings
### A2-AD-001：canonical paths 被保持
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 规划模块不会回退到旧 `manifest.json`、`trajectory/waypoints.json` 或根目录 `validation_report.json` 主合同。
- Recommended fix: 保持当前入口约束，并加入负样例测试。
- Verification method: 删除 `project_manifest.json` 后 loader 应拒绝；存在旧路径但无 canonical 路径时应拒绝。

### A2-AD-002：waypoint 字段映射符合要求
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 控制器可接收曲率和目标速度字段，轨迹拼接有稳定输入。
- Recommended fix: 增加 C++ loader 单元测试覆盖 `kappa/v_mps`。
- Verification method: 构造包含非零 kappa/v_mps 的样例并断言内部字段。

### A2-AD-003：validation 拒载逻辑已结构化
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: failed validation 或 `blocking_errors > 0` 的包会在加载阶段拒绝，降低使用非法路网的风险。
- Recommended fix: 增加 failed report 和 manifest/report 不一致负样例。
- Verification method: C++ 或 CLI loader 测试。

### A2-AD-004：checksum/hash runtime 校验仍不完整
- Severity: P1
- Status: Fail
- Impact on planning/control/vehicle operation: 损坏或篡改的 AD Package 可能在 ROS2 runtime 中被加载，只产生 warning。
- Recommended fix: 在 C++ 中实现 SHA-256，解析 `checksums.sha256` 和 `manifest.hashes`，并在 `roadnet.verify_checksums=true` 时拒绝 mismatch。
- Verification method: 篡改 `trajectory/waypoints.yaml`，确认 loader 拒绝加载。

### A2-AD-005：semantics 已加载但未完全使用
- Severity: P2
- Status: Partial
- Impact on planning/control/vehicle operation: task/parking 目标解析可用，但 speed-zone/no-go 等语义不影响路线和速度。
- Recommended fix: 将 semantic areas 转成 planner constraints 和 speed planner zone rules。
- Verification method: 构造 no-go/speed-zone 样例，检查 route 或目标速度变化。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /low_speed_av_planning/reload_roadnet ...`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test --packages-select low_speed_av_planning`

