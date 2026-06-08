# Launch 与 Bringup 第二轮审计

## Objective
审计 planning/control/bringup launch 默认值、package share 路径、sample AD Package 默认路径、配置安装和 override 能力。

## Status: Partial
静态检查显示 launch 默认值已从空路径改为 package share 路径，并且 bringup demo 默认传入 sample AD Package。ROS2 launch 实际运行未验证。

## Evidence
- planning launch 使用 `FindPackageShare`：`src/low_speed_av_planning/launch/planning.launch.py:5`、`src/low_speed_av_planning/launch/planning.launch.py:12`。
- control launch 使用 `FindPackageShare`：`src/low_speed_av_control/launch/control.launch.py:5`、`src/low_speed_av_control/launch/control.launch.py:12`。
- bringup demo 默认 sample AD Package：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:25` 至 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:31`。
- bringup demo 将 `roadnet.package_path` 传入 planning：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:39`。
- bringup 安装 launch/config/sample：`src/low_speed_av_bringup/CMakeLists.txt:6`。
- planning/control 安装 config/launch：`src/low_speed_av_planning/CMakeLists.txt:46`、`src/low_speed_av_control/CMakeLists.txt:43`。

## Findings
### A2-LB-001：launch 默认配置路径已修复
- Severity: P2
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 安装后默认 launch 更可能加载预期 YAML，而不是空参数启动。
- Recommended fix: ROS2 环境做无参数 launch 验证。
- Verification method: `ros2 launch low_speed_av_planning planning.launch.py` 后 dump 参数。

### A2-LB-002：bringup demo 默认 sample AD Package 已修复
- Severity: P2
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: demo launch 应能默认加载内置 sample roadnet，降低首次运行门槛。
- Recommended fix: 在 ROS2 环境确认 `RoadnetStatus.ready=true`。
- Verification method: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`。

### A2-LB-003：override 能力保留
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 使用者可用 launch arg 替换 config 和 roadnet package。
- Recommended fix: README 中补充 launch arg 示例。
- Verification method: 传入自定义 `roadnet_package_path`，检查参数生效。

### A2-LB-004：launch runtime 未验证
- Severity: P1
- Status: Not Verified
- Impact on planning/control/vehicle operation: 可能存在 launch substitution、安装路径或参数覆盖顺序问题。
- Recommended fix: ROS2 环境实际启动。
- Verification method: `ros2 launch`、`ros2 param dump`、topic/service 检查。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_planning planning.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_control control.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 param dump /low_speed_av_planning`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 param dump /low_speed_av_control`

