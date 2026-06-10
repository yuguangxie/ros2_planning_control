# Audit 5 Index

## Objective

列出第 5 轮完整功能模块审计文档。

## Scope

`docs/audits_5/`。

## Status

Pass。

## Evidence

- `rg --files docs/audits_5` 可列出本索引引用的 16 个第五轮审计文件。
- `reports/audit_5_full_module_function_audit_report.md` 是本轮审计的总报告。

## Documents

1. [AUDIT_5_SUMMARY.md](AUDIT_5_SUMMARY.md)
2. [INTERFACES_AUDIT.md](INTERFACES_AUDIT.md)
3. [ROADNET_AD_PACKAGE_AUDIT.md](ROADNET_AD_PACKAGE_AUDIT.md)
4. [PLANNING_MODULE_FUNCTION_AUDIT.md](PLANNING_MODULE_FUNCTION_AUDIT.md)
5. [CURRENT_POSE_START_PLANNING_AUDIT.md](CURRENT_POSE_START_PLANNING_AUDIT.md)
6. [CONTROL_MODULE_FUNCTION_AUDIT.md](CONTROL_MODULE_FUNCTION_AUDIT.md)
7. [SCU_OUTPUT_AND_SAFETY_AUDIT.md](SCU_OUTPUT_AND_SAFETY_AUDIT.md)
8. [SIMULATION_VISUALIZATION_AUDIT.md](SIMULATION_VISUALIZATION_AUDIT.md)
9. [LAUNCH_CONFIG_AND_PARAMETERS_AUDIT.md](LAUNCH_CONFIG_AND_PARAMETERS_AUDIT.md)
10. [DATAFLOW_INTEGRATION_AUDIT.md](DATAFLOW_INTEGRATION_AUDIT.md)
11. [TESTING_AND_OFFLINE_SMOKE_AUDIT.md](TESTING_AND_OFFLINE_SMOKE_AUDIT.md)
12. [ROS2_MANUAL_VALIDATION_PROCEDURE.md](ROS2_MANUAL_VALIDATION_PROCEDURE.md)
13. [ROS2_MANUAL_VALIDATION_CHECKLIST.md](ROS2_MANUAL_VALIDATION_CHECKLIST.md)
14. [RISK_REGISTER_5.md](RISK_REGISTER_5.md)
15. [FIX_PLAN_5.md](FIX_PLAN_5.md)

Report:

- [reports/audit_5_full_module_function_audit_report.md](../../reports/audit_5_full_module_function_audit_report.md)

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-IDX-001 | P3 | Pass | Required audit files created. | 审计材料可导航。 | 无。 | `rg --files docs/audits_5`。 |

## ROS2 Commands Run Or Skipped

SKIPPED_ROS2_UNAVAILABLE:

- This index does not require ROS2 commands.

## Remaining Uncertainty

无。
