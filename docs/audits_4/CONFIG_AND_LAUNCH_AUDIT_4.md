# 配置与 Launch 审计 4

## Objective（目标）
审计 control 与 bringup 配置是否包含 SCU 输出、LQR 参数、默认值和 launch 可用性，并确认内部 `/control/command` 是否只作为 debug/backward compatibility。

## Status（状态）
Partial。配置静态满足默认 SCU 输出和 LQR 默认算法；launch 实际加载与参数覆盖未在 ROS2 环境验证。

## Evidence（证据）
- control config 默认 `output.mode=scu_control_command`：`src/low_speed_av_control/config/control_params.yaml:10`。
- bringup config 默认 `output.mode=scu_control_command`：`src/low_speed_av_bringup/config/control_params.yaml:6`。
- control config SCU topic：`src/low_speed_av_control/config/control_params.yaml:19`。
- bringup config SCU topic：`src/low_speed_av_bringup/config/control_params.yaml:15`。
- control config 默认 LQR：`src/low_speed_av_control/config/control_params.yaml:23`。
- bringup config 默认 LQR：`src/low_speed_av_bringup/config/control_params.yaml:19`。
- SCU 限值、sign、stop gear、flags、lights：`src/low_speed_av_control/config/control_params.yaml:90` 到 `src/low_speed_av_control/config/control_params.yaml:98`，`src/low_speed_av_bringup/config/control_params.yaml:86` 到 `src/low_speed_av_bringup/config/control_params.yaml:94`。
- LQR Q/R 配置：`src/low_speed_av_control/config/control_params.yaml:59` 到 `src/low_speed_av_control/config/control_params.yaml:68`。
- bringup demo launch 加载 control params：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:42` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:47`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-CFG-001 | P3 | Pass | 默认控制输出是 SCU topic，不是内部 `/control/command`。 |
| A4-CFG-002 | P3 | Pass | 默认 SCU 限值、sign、stop gear、valid flags 和 lights 均有配置。 |
| A4-CFG-003 | P3 | Pass | 默认 `controller.algorithm` 已设置为 `lqr`。 |
| A4-CFG-004 | P2 | Not Verified | launch 后参数是否实际进入节点未验证。 |
| A4-CFG-005 | P3 | Partial | 根 README 仍保留旧默认 `controller.algorithm: pure_pursuit` 描述，可能与当前 config 默认 LQR 不一致。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
配置默认面向 SCU 输出，符合新增需求。README 旧默认可能误导人工验证。launch 未验证可能隐藏参数 namespace 或 share path 问题。

## Recommended fix（推荐修复）
- 在真实 ROS2 环境运行 `ros2 param get` 验证参数。
- 更新根 README，使默认控制器和 SCU 输出描述与当前配置一致。
- 若支持多场景，提供 bench-safe config 与 vehicle-live config。

## Verification method（验证方法）
- 静态读取 YAML 和 launch。
- 未执行 `ros2 launch` 与 `ros2 param get`。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 param get /low_speed_av_control output.mode`
- `ros2 param get /low_speed_av_control controller.algorithm`

