# 配置与 Launch 审计

## 目标
审计 YAML 默认配置、launch 文件、bringup 一致性和 sample package 可用性。

## 状态
部分通过。

## 证据
- planning config 的 `roadnet.package_path` 默认空，见 `src/low_speed_av_planning/config/planning_params.yaml:4`。
- planning 默认算法是 `astar`、`reference_line`、`curvature`，见 `src/low_speed_av_planning/config/planning_params.yaml:16` 至 `src/low_speed_av_planning/config/planning_params.yaml:30`。
- control 默认控制器/车辆模型是 `pure_pursuit` 和 `front_ackermann`，见 `src/low_speed_av_control/config/control_params.yaml:11` 至 `src/low_speed_av_control/config/control_params.yaml:18`。
- launch 文件默认参数路径为空，见 `src/low_speed_av_planning/launch/planning.launch.py:10` 至 `src/low_speed_av_planning/launch/planning.launch.py:20`，`src/low_speed_av_control/launch/control.launch.py:10` 至 `src/low_speed_av_control/launch/control.launch.py:20`，`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:11` 至 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:25`。
- bringup 安装 sample package，见 `src/low_speed_av_bringup/CMakeLists.txt:6`。

## 发现
### F-CL-001：默认算法配置符合要求
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：YAML 中有预期算法名称。
- 推荐修复：将所有配置字段接入节点运行行为。
- 验证方法：ROS2 环境中 dump 参数并比对 YAML。

### F-CL-002：launch 文件没有默认指向安装后的 config
- 严重级别：P2
- 状态：失败
- 对规划/控制/车辆运行影响：无参数启动 launch 时，节点可能不会加载预期 YAML。
- 推荐修复：使用 `get_package_share_directory` 默认定位安装后的 config 文件。
- 验证方法：无参数运行 launch，确认参数已加载。

### F-CL-003：bringup config 未设置 sample roadnet package path
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：demo launch 默认启动后 planning 的 `roadnet.package_path` 为空，无法直接进入 roadnet ready。
- 推荐修复：增加 bringup 专用 planning config，指向安装后的 sample package，或通过 launch substitution 传入 package path。
- 验证方法：demo launch 后 `RoadnetStatus.ready` 应为 true。

### F-CL-004：大量配置项未被节点消费
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：planner options、controller gains、smoother limits、vehicle limits 等配置可能让使用者误以为已经生效。
- 推荐修复：将配置读取到 typed runtime options，并为每类配置写行为测试。
- 验证方法：修改每类配置，断言输出行为变化。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 param dump /low_speed_av_planning`
