# Yunle Chassis 兼容性审计索引

## Objective

索引本轮 `low_speed_av_control` 与 `yunle_chassis` 兼容性审计输出。

## Scope

`docs/audits_chassis/` 下所有文件和总报告 `reports/control_to_yunle_chassis_compatibility_report.md`。

## Status: Pass

审计文档已按只读模式生成；未修改运行源码。

## Evidence

- `docs/audits_chassis/CHASSIS_MODULE_CODE_WALKTHROUGH.md`
- `docs/audits_chassis/CONTROL_TO_CHASSIS_COMPATIBILITY_AUDIT.md`
- `docs/audits_chassis/SCU_CONTROL_COMMAND_FIELD_MAPPING.md`
- `docs/audits_chassis/CHASSIS_DRIVER_CAN_MAPPING_AUDIT.md`
- `docs/audits_chassis/CONTROL_CHASSIS_LAUNCH_CONFIG_AUDIT.md`
- `docs/audits_chassis/ROS2_CHASSIS_INTEGRATION_VALIDATION_PROCEDURE.md`
- `docs/audits_chassis/RISK_REGISTER_CHASSIS_COMPATIBILITY.md`
- `docs/audits_chassis/FIX_PLAN_CHASSIS_COMPATIBILITY.md`
- `reports/control_to_yunle_chassis_compatibility_report.md`

## Recommended Reading Order

1. `CONTROL_TO_CHASSIS_COMPATIBILITY_AUDIT.md`
2. `SCU_CONTROL_COMMAND_FIELD_MAPPING.md`
3. `CHASSIS_DRIVER_CAN_MAPPING_AUDIT.md`
4. `CONTROL_CHASSIS_LAUNCH_CONFIG_AUDIT.md`
5. `ROS2_CHASSIS_INTEGRATION_VALIDATION_PROCEDURE.md`
6. `RISK_REGISTER_CHASSIS_COMPATIBILITY.md`
7. `FIX_PLAN_CHASSIS_COMPATIBILITY.md`
8. `CHASSIS_MODULE_CODE_WALKTHROUGH.md`

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-IDX-001 | P2 | 静态审计通过不等于真实 ROS2/底盘通过；下一步必须 bench-only 验证。 |

## Impact

为下一轮修复或台架验证提供依据。

## Recommended Fix

无源码修改建议在本文件执行；参考 `FIX_PLAN_CHASSIS_COMPATIBILITY.md`。

## Verification Method

确认所有文件存在并按人工验证流程执行。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 本轮未执行 ROS2 命令。
