# AD Package 兼容性审计 3

## Objective（目标）
审计工程是否遵循 Low Speed Roadnet AD Package v1.1 规范，优先使用 canonical paths，并避免旧路径作为 primary path。

## Status（状态）
Pass with Runtime Not Verified。源码和样例包均使用 v1.1 canonical paths，旧路径仅作为禁止项在脚本/文档中出现。真实 C++ 节点加载尚未在 ROS2 环境运行验证。

## Evidence（证据）
- loader 必须读取 `project_manifest.json`：`src/low_speed_av_planning/src/roadnet_loader.cpp:213` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:215`。
- schema/version 检查：`src/low_speed_av_planning/src/roadnet_loader.cpp:218` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:224`。
- canonical fallback 包含 `trajectory/waypoints.yaml`：`src/low_speed_av_planning/src/roadnet_loader.cpp:269`。
- waypoint mapping `kappa` 和 `v_mps`：`src/low_speed_av_planning/src/roadnet_loader.cpp:342` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:343`。
- `end_index_exclusive` 与 legacy inclusive `end_index` 支持：`src/low_speed_av_planning/src/roadnet_loader.cpp:372` 到 `src/low_speed_av_planning/src/roadnet_loader.cpp:375`。
- 样例 manifest schema/version：`src/low_speed_av_bringup/sample_ad_package/project_manifest.json:2` 到 `src/low_speed_av_bringup/sample_ad_package/project_manifest.json:4`。
- 样例 manifest files 使用 canonical path：`src/low_speed_av_bringup/sample_ad_package/project_manifest.json:47` 到 `src/low_speed_av_bringup/sample_ad_package/project_manifest.json:60`。
- 样例 validation passed：`src/low_speed_av_bringup/sample_ad_package/validation/validation_report.json:4`、`src/low_speed_av_bringup/sample_ad_package/validation/validation_report.json:7`。
- Python 样例校验禁止旧路径：`scripts/validate_sample_ad_package.py:62` 到 `scripts/validate_sample_ad_package.py:64`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-AD-001 | P3 | Pass | `project_manifest.json` 是 loader primary manifest。 |
| A3-AD-002 | P3 | Pass | `trajectory/waypoints.yaml` 与 `validation/validation_report.json` 为 canonical 输入。 |
| A3-AD-003 | P3 | Pass | `end_index_exclusive` 优先，legacy `end_index` inclusive fallback 已存在。 |
| A3-AD-004 | P3 | Pass | `kappa -> kappa_1pm` 与 `v_mps -> target_speed_mps` 映射已实现。 |
| A3-AD-005 | P2 | Not Verified | C++ loader 对真实 ZIP 解包目录的所有边界情况尚未通过编译级测试覆盖。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
AD Package 合同基本满足，降低了使用旧 schema 导致错误路线或速度的风险。剩余风险是未通过 C++ 运行时测试验证异常路径。

## Recommended fix（推荐修复）
- 添加 C++ loader 测试覆盖 valid sample、failed validation、bad index、missing file、checksum mismatch。
- 在真实 ROS2 节点中加载 sample AD Package 并检查 `/planning/roadnet_status`。

## Verification method（验证方法）
- 已运行 `validate_sample_ad_package.py`，输出 `AD Package OK`。
- 已运行 `offline_remaining_fixes_smoke.py`，覆盖 checksum、bad validation、bad index。
- 未运行 C++ loader 单元测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 topic echo /planning/status`
- `ros2 topic echo /planning/roadnet_status`

