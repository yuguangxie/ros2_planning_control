# SCU 输出兼容性审计

## Objective（目标）
审计控制包是否正确依赖 ROS2 `chassis_interfaces`，是否默认发布 Yunle SCU topic，并确认未引入 ROS1 `catkin`、`roscpp` 或不存在的 `scu_drive_mode_request` 字段。

## Status（状态）
Partial。静态依赖、include、topic 和 publisher 均符合合同；真实 ROS2 构建和接口生成未验证。

## Evidence（证据）
- `find_package(rclcpp REQUIRED)`：`src/low_speed_av_control/CMakeLists.txt:9`。
- `find_package(builtin_interfaces REQUIRED)`：`src/low_speed_av_control/CMakeLists.txt:10`。
- `find_package(chassis_interfaces REQUIRED)`：`src/low_speed_av_control/CMakeLists.txt:13`。
- `ament_target_dependencies` 包含 `chassis_interfaces`：`src/low_speed_av_control/CMakeLists.txt:35` 到 `src/low_speed_av_control/CMakeLists.txt:40`。
- export dependencies 包含 `chassis_interfaces`：`src/low_speed_av_control/CMakeLists.txt:56`。
- `package.xml` 依赖：`src/low_speed_av_control/package.xml:11`、`src/low_speed_av_control/package.xml:14`。
- ROS2 generated include：`src/low_speed_av_control/include/low_speed_av_control/control_node.hpp:8`，`src/low_speed_av_control/include/low_speed_av_control/scu_command_mapper.hpp:6`。
- SCU publisher 类型：`src/low_speed_av_control/include/low_speed_av_control/control_node.hpp:75`。
- 默认 topic 参数：`src/low_speed_av_control/src/control_node.cpp:22`。
- 静态检索未发现源码引入 `catkin`、`roscpp`、`scu_drive_mode_request`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-SCUC-001 | P3 | Pass | 控制包使用 ROS2/ament CMake 依赖，没有 ROS1 构建系统迹象。 |
| A4-SCUC-002 | P3 | Pass | `chassis_interfaces` 已在 CMake、package.xml、include 和 publisher 类型中接入。 |
| A4-SCUC-003 | P3 | Pass | 默认 topic 正确：`/yunle_chassis/control/scu_control_command`。 |
| A4-SCUC-004 | P3 | Pass | 未添加或引用 `scu_drive_mode_request` 字段。 |
| A4-SCUC-005 | P2 | Not Verified | `chassis_interfaces/msg/ScuControlCommand` 是否在目标 workspace 存在且字段完全匹配，未通过 `ros2 interface show` 验证。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
若 `chassis_interfaces` 不存在或字段名与假设不一致，控制包会在编译期失败，或无法发布底盘命令。静态层面未发现 ROS1 依赖污染。

## Recommended fix（推荐修复）
- 在真实 ROS2 环境运行 `ros2 interface show chassis_interfaces/msg/ScuControlCommand`。
- 在 `colcon build` 中确认 `chassis_interfaces` 可被 rosdep/workspace 解析。
- 若 CMake target 传播不充分，补充 executable 级 `ament_target_dependencies(control_node chassis_interfaces low_speed_av_interfaces)`。

## Verification method（验证方法）
- 静态读取 CMake/package/include。
- 静态 `rg` 搜索 ROS1 关键词和 SCU 字段。
- ROS2 编译和接口显示未执行。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 interface show chassis_interfaces/msg/ScuControlCommand`
- `colcon build --packages-select low_speed_av_control`

