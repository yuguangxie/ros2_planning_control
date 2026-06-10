# Roadnet AD Package Audit

## Objective

审计 `roadnet_ad_package_20260610T012525Z` 是否满足当前 RoadnetLoader、规划模块和仿真可视化模块的输入要求。

## Scope

- `roadnet_ad_package_20260610T012525Z`
- `scripts/validate_sample_ad_package.py`
- `docs/ROADNET_AD_PACKAGE_20260610T012525Z_ANALYSIS.md`

## Status

Pass by offline validation.

## Evidence

- 离线校验输出：`AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)`。
- Manifest schema/version：`low_speed_roadnet_ad_package 1.1.0`。
- Manifest validation：`status=warning`、`blocking_errors=0`。
- Validation report summary：16 nodes、22 edges、496 waypoints、4 areas、32 warnings、0 blocking errors。
- `docs/ROADNET_AD_PACKAGE_20260610T012525Z_ANALYSIS.md` 已给出逐文件统计和 checksum 结果。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-RN-001 | P3 | Pass | canonical 文件齐全，未发现旧主路径。 | 可以作为当前规划输入。 | 无。 | `validate_sample_ad_package.py`。 |
| AUD5-RN-002 | P2 | Partial | validation 是 warning，包含高曲率/曲率连续性 warning。 | 实车跟踪可能需要更低速度和转角监控。 | 路网编辑器平滑高曲率路径，实车前低速验证。 | RViz + 控制输出观察。 |
| AUD5-RN-003 | P3 | Pass | checksum 全部匹配。 | `roadnet.verify_checksums=true` 可加载。 | 保持跨平台 LF/二进制一致。 | 修改任一文件后应触发 mismatch。 |
| AUD5-RN-004 | P2 | Partial | no-go/speed-zone 当前未覆盖 waypoint。 | 无法用该包验证语义阻断/限速实际效果。 | 增加语义测试包。 | `offline_remaining_fixes_smoke.py` 或新语义包。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 service call /low_speed_av_planning/reload_roadnet ...`
- `ros2 topic echo /planning/roadnet_status`

## Remaining Uncertainty

Roadnet 文件离线可用，但尚未在真实 planning node 中加载验证。

