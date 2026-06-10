# Interfaces Audit

## Objective

审计自定义 msg/srv 与外部 `chassis_interfaces/msg/ScuControlCommand` 的使用是否满足当前规划、控制、仿真和 SCU 输出合同。

## Scope

- `src/low_speed_av_interfaces/msg/*.msg`
- `src/low_speed_av_interfaces/srv/*.srv`
- `src/low_speed_av_interfaces/CMakeLists.txt`
- `src/low_speed_av_control/package.xml`
- `src/low_speed_av_control/CMakeLists.txt`

## Status

Pass by static audit, Not Verified by ROS2 runtime.

## Evidence

- `PlanRoute.srv` 保留显式起终点字段：`src/low_speed_av_interfaces/srv/PlanRoute.srv:2` 到 `src/low_speed_av_interfaces/srv/PlanRoute.srv:6`。
- `PlanRoute.srv` 返回 route，轨迹由 topic 发布：`src/low_speed_av_interfaces/srv/PlanRoute.srv:8` 到 `src/low_speed_av_interfaces/srv/PlanRoute.srv:11`。
- `ReloadRoadnet.srv` 只接收 `package_path`：`src/low_speed_av_interfaces/srv/ReloadRoadnet.srv:2`。
- `SetPlannerAlgorithm.srv` 字段匹配 planning code：`src/low_speed_av_interfaces/srv/SetPlannerAlgorithm.srv:2` 到 `src/low_speed_av_interfaces/srv/SetPlannerAlgorithm.srv:4`。
- `ControlCommand.msg` 包含 front/rear steering：`src/low_speed_av_interfaces/msg/ControlCommand.msg:6` 到 `src/low_speed_av_interfaces/msg/ControlCommand.msg:8`。
- `TrajectoryPoint.msg` 使用 SI 单位并带 gear：`src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:1` 到 `src/low_speed_av_interfaces/msg/TrajectoryPoint.msg:17`。
- `low_speed_av_control` 声明 `chassis_interfaces` 依赖：`src/low_speed_av_control/package.xml:14`、`src/low_speed_av_control/CMakeLists.txt:13`。
- 静态 `rg` 未在源码中发现 `catkin`、`roscpp`、`scu_drive_mode_request`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-IF-001 | P3 | Pass | PlanRoute 未改 srv，旧客户端显式 start 调用保持兼容。 | 旧任务系统可继续使用。 | 无。 | `ros2 interface show low_speed_av_interfaces/srv/PlanRoute`。 |
| AUD5-IF-002 | P2 | Partial | current-pose start 行为不体现在 srv 字段中，而是“start 为空 + 参数启用”的语义。 | 客户端需要读文档，否则可能误以为空 start 是错误。 | 在接口注释或 README 中补充空 start 语义。 | `PlanRoute` 空 start 集成测试。 |
| AUD5-IF-003 | P3 | Pass | ControlCommand 与 TrajectoryPoint 均有 gear，但 enum 注释为 DRIVE/REVERSE/PARK，与 SCU D/N/R 映射需通过 mapper 文档理解。 | 语义可用，但存在理解成本。 | 保持 mapper 文档清晰，后续可统一 gear enum。 | 检查 mapper 和 SCU 输出。 |
| AUD5-IF-004 | P1 | Not Verified | `chassis_interfaces/msg/ScuControlCommand` 真实接口未在当前环境 `ros2 interface show` 验证。 | 若目标包字段不同会导致编译或运行失败。 | 在 ROS2 环境执行接口验证。 | `ros2 interface show chassis_interfaces/msg/ScuControlCommand`。 |

## ROS2 Commands Run Or Skipped

Run:

- `rg -n "catkin|roscpp|scu_drive_mode_request|ScuControlCommand|PlanRoute|ReloadRoadnet" .`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 interface show low_speed_av_interfaces/srv/PlanRoute`
- `ros2 interface show chassis_interfaces/msg/ScuControlCommand`
- `colcon build --packages-select low_speed_av_interfaces`

## Remaining Uncertainty

接口静态一致，但 ROS2 生成代码、安装和跨包依赖未验证。

