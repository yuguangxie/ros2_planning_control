# 第二轮审计索引

## Objective
列出第二轮审计生成的所有文档、用途、总体状态和阅读顺序。

## Status: Pass
`docs/audits_2/` 已包含用户要求的 13 个中文审计文件。

## Evidence
- 第二轮目录：`docs/audits_2/`。
- 已创建文件：`AUDIT_2_SUMMARY.md`、`REGRESSION_FROM_AUDIT_1.md`、`AD_PACKAGE_COMPATIBILITY_AUDIT_2.md`、`ROADNET_LOADER_AUDIT_2.md`、`PLANNING_MODULE_AUDIT_2.md`、`CONTROL_MODULE_AUDIT_2.md`、`SAFETY_AND_COMMAND_AUDIT_2.md`、`INTERFACES_TOPICS_CONFIG_AUDIT_2.md`、`LAUNCH_BRINGUP_AUDIT_2.md`、`TESTING_WITHOUT_ROS2_AUDIT_2.md`、`RISK_REGISTER_2.md`、`FIX_PLAN_2.md`、`AUDIT_2_INDEX.md`。
- 章节复核命令：`rg -n "^## Objective|^## Status|^## Evidence|^## Findings|SKIPPED_ROS2_UNAVAILABLE" docs\audits_2`。

## Audit files
| File | Purpose | Status |
|---|---|---|
| `AUDIT_2_SUMMARY.md` | 第二轮总体结论、主要修复、剩余风险和离线检查结果。 | Partial |
| `REGRESSION_FROM_AUDIT_1.md` | 第一轮问题逐项回归对照表。 | Partial |
| `AD_PACKAGE_COMPATIBILITY_AUDIT_2.md` | AD Package v1.1 canonical paths、字段映射、validation、semantics、checksum。 | Partial |
| `ROADNET_LOADER_AUDIT_2.md` | RoadnetLoader 结构化解析、边界检查、semantics 和 checksum 行为。 | Partial |
| `PLANNING_MODULE_AUDIT_2.md` | 规划服务、工厂、路线/轨迹发布和失败行为。 | Partial |
| `CONTROL_MODULE_AUDIT_2.md` | 控制正常链路、控制器、车辆模型、限幅和平滑。 | Partial |
| `SAFETY_AND_COMMAND_AUDIT_2.md` | estop、timeout、空轨迹停车、Ackermann 和 NaN/Inf guard。 | Partial |
| `INTERFACES_TOPICS_CONFIG_AUDIT_2.md` | msg/srv、话题默认值和可配置性。 | Partial |
| `LAUNCH_BRINGUP_AUDIT_2.md` | launch 默认路径、bringup sample AD Package 和安装配置。 | Partial |
| `TESTING_WITHOUT_ROS2_AUDIT_2.md` | 无 ROS2 离线脚本结果和测试缺口。 | Partial |
| `RISK_REGISTER_2.md` | 第二轮风险登记表。 | Partial |
| `FIX_PLAN_2.md` | 下一阶段修复计划和建议 Codex prompt。 | Pass |
| `AUDIT_2_INDEX.md` | 本索引。 | Pass |

## Recommended reading order
1. `AUDIT_2_SUMMARY.md`
2. `REGRESSION_FROM_AUDIT_1.md`
3. `RISK_REGISTER_2.md`
4. `FIX_PLAN_2.md`
5. 需要深入时阅读对应模块审计。

## Findings
### A2-IDX-001
- Severity: P3
- Finding: 文档集完整，所有文件均使用中文审计输出，并包含 ROS2 unavailable 跳过项。
- Impact on planning/control/vehicle operation: 后续开发者可直接从风险和修复计划继续推进。
- Recommended fix: 每轮优化后追加 `docs/audits_N/`，保持可追溯。
- Verification method: 检查文件存在和章节完整性。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
