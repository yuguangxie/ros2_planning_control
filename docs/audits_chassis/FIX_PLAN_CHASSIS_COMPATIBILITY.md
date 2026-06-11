# Chassis 兼容性修复计划

## Objective

给出后续如果进入修复阶段，应优先修改的文件和验收标准。本轮不修改源码。

## Scope

只覆盖 control 与 `yunle_chassis/chassis_driver` 对接相关问题。

## Status: Plan Only

## Evidence

- 兼容审计结论见 `docs/audits_chassis/CONTROL_TO_CHASSIS_COMPATIBILITY_AUDIT.md`。
- CAN 映射审计结论见 `docs/audits_chassis/CHASSIS_DRIVER_CAN_MAPPING_AUDIT.md`。
- 风险登记见 `docs/audits_chassis/RISK_REGISTER_CHASSIS_COMPATIBILITY.md`。

## Fix Plan

| Phase | Target | Files likely affected | Exact fix description | Acceptance criteria | Suggested Codex prompt |
|---|---|---|---|---|---|
| 1 | Bench 验证文档执行 | 无源码 | 按人工流程验证 publisher/subscriber/type/CAN raw | topic pub=1 sub=1，0x121 正确 | “按 docs/audits_chassis/ROS2_CHASSIS_INTEGRATION_VALIDATION_PROCEDURE.md 执行并记录结果” |
| 2 | Driver command watchdog | `src/yunle_chassis/chassis_driver/src/chassis_driver_node.cpp`, `control_command_bridge.cpp`, config | 增加最近 SCU 命令时间和 timeout stop 策略 | control 停止发布后 driver 发送安全停车或进入禁能状态 | “为 chassis_driver 增加 SCU command timeout watchdog” |
| 3 | 可选联合 bringup | `src/low_speed_av_bringup/launch/*.py`, `package.xml` | 增加可选 `launch_chassis_driver`，默认 false | 手工启用后一起启动 planning/control/driver | “增加 bench-only 可选 chassis_driver bringup” |
| 4 | 参数一致性诊断 | control 或 driver config/docs | 启动时输出 control/driver SCU limit 对比或新增脚本 | 不一致时有清晰 warning | “增加 SCU 参数一致性检查脚本和文档” |
| 5 | CAN 协议确认 | docs | 明确 `SCU_Drive_Mode_Request=1` 供应商含义、周期、watchdog | 文档与底盘协议一致 | “根据供应商协议更新 Yunle CAN 0x121 说明” |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-FIX-001 | P1 | 第一优先级不是改 control mapper，而是完成 driver 侧 watchdog/周期策略确认。 |
| CHAS-FIX-002 | P2 | 需要防止 keyboard 与 control 双 publisher。 |

## Impact

执行后可提升底盘联调安全性和 launch 可用性。

## Recommended Fix

等待用户确认后再进入源码修复阶段。

## Verification Method

每个 phase 都必须在 Ubuntu ROS2 环境用 `colcon build`、`ros2 topic info`、`ros2 topic echo` 和 bench CAN 工具验证。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 当前仅生成计划。
