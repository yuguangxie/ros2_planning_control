# 控制节点集成审计

## Objective（目标）
审计控制节点是否在 limiter、smoother、Ackermann 转换和 finite guard 后发布 SCU 命令，并确认安全状态、timeout、空轨迹和内部 debug 输出模式。

## Status（状态）
Partial。源码显示控制节点将正常/安全命令统一经过 `finalize_command` 后发布，并根据 `output.mode` 控制内部与 SCU 输出；但 ROS2 定时器、参数加载、topic 发布未验证。

## Evidence（证据）
- SCU publisher 成员：`src/low_speed_av_control/include/low_speed_av_control/control_node.hpp:75`。
- 默认 `output.mode`：`src/low_speed_av_control/src/control_node.cpp:24`。
- SCU publisher 创建：`src/low_speed_av_control/src/control_node.cpp:97` 到 `src/low_speed_av_control/src/control_node.cpp:98`。
- runtime 读取 output mode：`src/low_speed_av_control/src/control_node.cpp:121`。
- controller 输出曲率后 vehicle model 转前后轮：`src/low_speed_av_control/src/control_node.cpp:266` 到 `src/low_speed_av_control/src/control_node.cpp:269`。
- limiter+smoother：`src/low_speed_av_control/src/control_node.cpp:280` 到 `src/low_speed_av_control/src/control_node.cpp:282`。
- estop、定位超时、轨迹超时、空轨迹停车：`src/low_speed_av_control/src/control_node.cpp:288` 到 `src/low_speed_av_control/src/control_node.cpp:308`。
- 内部输出条件：`src/low_speed_av_control/src/control_node.cpp:367` 到 `src/low_speed_av_control/src/control_node.cpp:375`，`src/low_speed_av_control/src/control_node.cpp:379` 到 `src/low_speed_av_control/src/control_node.cpp:386`。
- NaN guard：`src/low_speed_av_control/src/command_limiter.cpp:17` 到 `src/low_speed_av_control/src/command_limiter.cpp:36`。
- smoother 对 emergency stop 置零：`src/low_speed_av_control/src/command_smoother.cpp:20` 到 `src/low_speed_av_control/src/command_smoother.cpp:28`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-CNI-001 | P3 | Pass | SCU 发布发生在 `finalize_command` 后，符合“最终输出层转换”要求。 |
| A4-CNI-002 | P3 | Pass | 安全停车路径经过 limiter/smoother 后进入 mapper，mapper 对 emergency stop 输出 brake stop。 |
| A4-CNI-003 | P3 | Pass | `output.mode=scu_control_command` 默认只发布 SCU，`internal/both` 才保留 `/control/command`。 |
| A4-CNI-004 | P2 | Not Verified | ROS2 参数嵌套 YAML 到 dotted parameter 的加载未通过 launch 验证。 |
| A4-CNI-005 | P2 | Not Verified | control node executable 是否因 `chassis_interfaces` include/target 传播成功编译未验证。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
正常链路和安全链路静态上能输出 SCU 命令。若参数未加载或编译目标依赖传播失败，节点可能无法启动或不使用期望 topic。

## Recommended fix（推荐修复）
- 在 ROS2 环境运行 launch 并检查 `/low_speed_av_control` 参数。
- 若构建失败，为 `control_node` executable 显式添加 `chassis_interfaces` 依赖。
- 添加 ROS2 launch smoke 或组件级测试验证 `output.mode`。

## Verification method（验证方法）
- 静态核查 control node。
- 已运行 Python SCU/LQR smoke。
- 未执行 ROS2 launch/topic。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 param get /low_speed_av_control topics.scu_command_topic`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`

