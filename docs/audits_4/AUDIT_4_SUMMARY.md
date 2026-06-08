# 第四轮审计总览：Yunle SCU 输出与 LQR 升级

## Objective（目标）
审计新增 Yunle SCU ROS2 底盘输出与升级后的 LQR 控制器，确认静态实现是否满足 `chassis_interfaces/msg/ScuControlCommand`、安全映射、单位转换和 Riccati LQR 合同，并给出人工 ROS2 验证流程。

## Status（状态）
Partial。静态审计和离线 smoke 通过，SCU 映射总体符合合同，LQR 已从增益骨架升级为 Riccati 迭代控制器。但是当前 Windows 环境没有 ROS2、`colcon`、`ros2`，因此 ROS2 编译、接口生成、launch、topic/service 和真实底盘驱动运行均为 Not Verified。

## Evidence（证据）
- ROS2/ament 依赖：`src/low_speed_av_control/CMakeLists.txt:8` 到 `src/low_speed_av_control/CMakeLists.txt:14`。
- `chassis_interfaces` 在 target deps：`src/low_speed_av_control/CMakeLists.txt:35` 到 `src/low_speed_av_control/CMakeLists.txt:40`。
- `package.xml` 依赖：`src/low_speed_av_control/package.xml:11`、`src/low_speed_av_control/package.xml:14`。
- SCU publisher：`src/low_speed_av_control/src/control_node.cpp:97` 到 `src/low_speed_av_control/src/control_node.cpp:98`。
- 默认 SCU topic：`src/low_speed_av_control/src/control_node.cpp:22`，`src/low_speed_av_control/config/control_params.yaml:19`。
- SCU mapper 挡位、单位与安全停车：`src/low_speed_av_control/src/scu_command_mapper.cpp:10` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:13`，`src/low_speed_av_control/src/scu_command_mapper.cpp:50` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:57`，`src/low_speed_av_control/src/scu_command_mapper.cpp:89` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:97`。
- LQR Riccati 迭代：`src/low_speed_av_control/src/lqr_controller.cpp:100` 到 `src/low_speed_av_control/src/lqr_controller.cpp:134`。
- LQR 曲率前馈与输出曲率：`src/low_speed_av_control/src/lqr_controller.cpp:149` 到 `src/low_speed_av_control/src/lqr_controller.cpp:158`。
- 离线 SCU/LQR smoke 通过：`scripts/offline_scu_lqr_smoke.py:173` 到 `scripts/offline_scu_lqr_smoke.py:246`。
- ROS2 不可用：`scripts/check_ros2_env.ps1` 输出 `SKIPPED_ROS2_UNAVAILABLE: colcon not found` 和 `ros2 not found`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-SUM-001 | P3 | Pass | 控制包保持 ROS2/ament 结构，未在源码中引入 `catkin` 或 `roscpp`。 |
| A4-SUM-002 | P3 | Pass | 默认底盘输出 topic 为 `/yunle_chassis/control/scu_control_command`，类型为 `chassis_interfaces/msg/ScuControlCommand`。 |
| A4-SUM-003 | P3 | Pass | SCU mapper 对速度、转角、挡位、安全停车有独立复用层。 |
| A4-SUM-004 | P2 | Partial | 内部 gear 注释只列出 `0 UNKNOWN, 1 DRIVE, 2 REVERSE, 3 PARK`，但 mapper 使用 `4` 表示 neutral；neutral 语义需在接口文档和测试中统一。 |
| A4-SUM-005 | P2 | Not Verified | `chassis_interfaces` 在真实 ROS2 workspace 中是否可解析、编译、生成消息未验证。 |
| A4-SUM-006 | P2 | Partial | LQR 为真实 Riccati 实现且离线通过，但尚未通过 ROS2/C++ 单元测试、仿真或实车调参验证。 |
| A4-SUM-007 | P1 | Not Verified | SCU 输出没有在真实底盘驱动或 bench 环境确认，不可直接视为车辆级通过。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
静态实现降低了非法挡位、负速度、非有限数值和安全停车映射风险。剩余最大风险是 ROS2/底盘运行时未验证，可能导致编译失败、消息字段不匹配、节点参数未加载或底盘驱动接收行为不符合预期。

## Recommended fix（推荐修复）
1. 在真实 ROS2 环境执行 `ROS2_MANUAL_VALIDATION_PROCEDURE.md`。
2. 明确内部 gear neutral 编码，建议在接口注释中加入 neutral 或让 mapper 使用已有内部枚举。
3. 为 `ScuCommandMapper` 与 `LqrController` 添加 C++ gtest，减少 Python 镜像测试与真实 C++ 行为偏差。
4. 在 bench 或 wheels-off 条件下验证 Yunle 底盘驱动实际接收 `/yunle_chassis/control/scu_control_command`。

## Verification method（验证方法）
- 已执行 `uv run python`：`validate_expected_tree.py` 和 `offline_scu_lqr_smoke.py` 通过；依赖 PyYAML 的脚本在无 `--with pyyaml` 时失败。
- 已执行 `uv run --with pyyaml python`：`validate_sample_ad_package.py`、`offline_algorithm_smoke.py`、`offline_remaining_fixes_smoke.py` 通过。
- 已执行 FreeCAD Python 离线脚本：expected tree、sample AD Package、offline algorithm smoke、remaining fixes smoke、SCU/LQR smoke，全部通过。
- 已执行静态 `rg` 检索：未发现源码中 `catkin`、`roscpp`、`scu_drive_mode_request`。
- 已执行 `scripts/check_ros2_env.ps1`：ROS2 命令跳过。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `colcon test`
- `colcon test-result --verbose`
- `ros2 interface show chassis_interfaces/msg/ScuControlCommand`
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`
