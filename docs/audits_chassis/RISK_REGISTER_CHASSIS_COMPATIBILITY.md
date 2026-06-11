# Chassis 兼容性风险登记

## Objective

登记 `low_speed_av_control` 与 `yunle_chassis/chassis_driver` 对接后的剩余风险。

## Scope

topic/type、字段映射、单位范围、CAN 编码、launch/config、bench 验证与实车安全。

## Status: Partial

无静态 P0/P1 阻塞，但存在多个必须在 bench-only / wheels-off 环境验证的 P2 风险。

## Evidence

- driver 没有本地 0x121 周期发送：`src/yunle_chassis/docs/ros2_topic_reference.md:417`。
- driver 订阅 callback 收到一条 ROS 消息立即发送一次：`src/yunle_chassis/docs/ros2_topic_reference.md:77`。
- driver 未发现控制超时保护：`src/yunle_chassis/docs/ros2_topic_reference.md:418`。
- bringup demo 不启动 chassis_driver：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:32` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:48`。

## Risk Register

| ID | Title | Severity | Probability | Affected module | Evidence | Impact | Recommended mitigation | Priority |
|---|---|---|---|---|---|---|---|---|
| CHAS-RISK-001 | 未经 bench 验证直接实车运动 | P0 | Medium | 全系统 | 本轮未运行 ROS2/底盘 | 可能造成非预期运动 | 仅 bench-only/wheels-off；完成 checklist 后再考虑低速封闭场地 | 最高 |
| CHAS-RISK-002 | driver 无本地控制超时停车 | P1 | Medium | chassis_driver | `src/yunle_chassis/docs/ros2_topic_reference.md:418` | 上游 control 崩溃时 driver 不自动 stop | 增加 command watchdog 或底盘侧确认已有 watchdog | 高 |
| CHAS-RISK-003 | driver 不主动周期重发 0x121 | P2 | Medium | chassis_driver | `src/yunle_chassis/docs/ros2_topic_reference.md:417` | 若 control 频率低或抖动，底盘可能超时 | 记录 topic hz，必要时 driver 增加周期发布最近安全命令 | 高 |
| CHAS-RISK-004 | bringup 未启动 chassis_driver | P2 | High | launch/config | `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:32` 到 `:48` | 误以为已连底盘但无 subscriber | 新增可选 chassis launch 或文档强制两步启动 | 中 |
| CHAS-RISK-005 | control/driver 限幅参数未来被改成不一致 | P2 | Medium | control/driver config | control 27 deg，driver 27 deg 当前匹配 | 超限可能被 driver 归零造成突变 | 增加启动诊断或共享参数检查 | 中 |
| CHAS-RISK-006 | keyboard 和 control 同时发布同一 SCU topic | P2 | Low | chassis_driver tools | `keyboard_scu_control_node` 也发布同 topic | 双 publisher 导致命令争用 | bench 文档要求 publisher count=1 | 中 |
| CHAS-RISK-007 | Drive mode 值命名需和供应商确认 | P2 | Medium | CAN protocol | driver 固定值 1，DBC 文本需确认 | 底盘模式理解错误 | 和底盘供应商确认 `SCU_Drive_Mode_Request=1` 含义 | 中 |
| CHAS-RISK-008 | `src/yunle_chassis` 未跟踪 | P3 | High | repo | `git status --short` 显示 `?? src/yunle_chassis/` | 远端可能缺少 driver 源码 | 确认后 git add/commit/push | 中 |

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-RISK-FIND-001 | P1 | driver 侧没有发现 command timeout stop，实车前需修复或确认底盘自带安全机制。 |
| CHAS-RISK-FIND-002 | P2 | 当前静态兼容不等于 ROS2 runtime 已验证。 |

## Impact

影响真实车辆安全、台架验证可信度和部署完整性。

## Recommended Fix

优先做 bench-only 验证；下一轮修复可聚焦 driver watchdog、联合 bringup 和参数一致性诊断。

## Verification Method

按 `ROS2_CHASSIS_INTEGRATION_VALIDATION_PROCEDURE.md` 执行，并补充断开 control publisher 后的 driver 行为观察。

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 未执行 ROS2/bench 验证。
