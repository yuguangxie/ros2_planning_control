# 工程结构审计 3

## Objective（目标）
审计四个 ROS2 包的结构、CMake、package.xml、依赖声明和 install 规则是否满足规划与控制分离的架构要求。

## Status（状态）
Partial。四包结构和职责边界清晰，CMake/install 规则基本完整；但由于 ROS2 环境不可用，CMake 配置、依赖解析和安装布局没有实际编译验证。

## Evidence（证据）
- 接口包生成 msg/srv：`src/low_speed_av_interfaces/CMakeLists.txt:8` 到 `src/low_speed_av_interfaces/CMakeLists.txt:19`。
- planning 包库和节点：`src/low_speed_av_planning/CMakeLists.txt:15`、`src/low_speed_av_planning/CMakeLists.txt:37` 到 `src/low_speed_av_planning/CMakeLists.txt:39`。
- planning 包 install include/config/launch：`src/low_speed_av_planning/CMakeLists.txt:41` 到 `src/low_speed_av_planning/CMakeLists.txt:46`。
- control 包库和节点：`src/low_speed_av_control/CMakeLists.txt:14`、`src/low_speed_av_control/CMakeLists.txt:34` 到 `src/low_speed_av_control/CMakeLists.txt:36`。
- control 包 install include/config/launch：`src/low_speed_av_control/CMakeLists.txt:38` 到 `src/low_speed_av_control/CMakeLists.txt:43`。
- bringup 安装 launch/config/sample：`src/low_speed_av_bringup/CMakeLists.txt:6`。
- planning `package.xml` 声明 `rclcpp`、`geometry_msgs`、`low_speed_av_interfaces`、`yaml-cpp`：`src/low_speed_av_planning/package.xml:9` 到 `src/low_speed_av_planning/package.xml:16`。
- control `package.xml` 声明控制依赖：`src/low_speed_av_control/package.xml:9` 到 `src/low_speed_av_control/package.xml:15`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-PS-001 | P3 | Pass | 四包目录满足 `interfaces/planning/control/bringup` 分离要求。 |
| A3-PS-002 | P2 | Not Verified | CMake 和 package.xml 未经过 `colcon build` 验证。 |
| A3-PS-003 | P2 | Not Verified | `yaml-cpp` 链接方式为 `target_link_libraries(${PROJECT_NAME} yaml-cpp)`，不同 ROS2/系统环境可能要求 `yaml-cpp::yaml-cpp` 或 rosdep 系统包，需真实构建确认。 |
| A3-PS-004 | P3 | Pass | bringup 安装 sample AD Package，利于安装后 demo 默认路径。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
结构风险较低。若 CMake 或依赖在 ROS2 环境解析失败，将导致节点无法构建，进而阻塞规划、控制和集成测试。

## Recommended fix（推荐修复）
- 在目标 ROS2 发行版中运行 `colcon build` 并修正 `yaml-cpp` 目标名或 rosdep 键。
- 添加 CI 或 Dockerfile 固定 ROS2 构建环境。
- 保持四包职责边界，不将 control 逻辑放入 planning 包。

## Verification method（验证方法）
- 静态读取 CMakeLists 和 package.xml。
- 已运行 expected tree 离线检查并通过。
- ROS2 编译验证未执行。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build`
- `colcon test`
- `colcon test-result --verbose`

