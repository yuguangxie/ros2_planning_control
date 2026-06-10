# Simulation Visualization Audit

## Objective

审计新增 `low_speed_av_simulation` 包的结构、节点、topic、服务、RViz 配置和安全文档。

## Scope

- `src/low_speed_av_simulation`
- `docs/SIMULATION_VISUALIZATION_USAGE.md`
- `docs/LOCALIZATION_POSE_SIMULATION_GUIDE.md`

## Status

Partial。源码结构和离线数据逻辑通过，ROS2 build/RViz 未验证。

## Evidence

- package 是 ament：`src/low_speed_av_simulation/package.xml:23`。
- CMake 依赖 rclcpp、geometry_msgs、nav_msgs、std_srvs、visualization_msgs、planning：`src/low_speed_av_simulation/CMakeLists.txt:8` 到 `src/low_speed_av_simulation/CMakeLists.txt:16`。
- 两个 executable：`src/low_speed_av_simulation/CMakeLists.txt:18`、`src/low_speed_av_simulation/CMakeLists.txt:30`。
- Marker/Path publishers：`src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:98` 到 `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:104`。
- Route/trajectory/pose subscriptions：`src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:107` 到 `src/low_speed_av_simulation/src/roadnet_visualization_node.cpp:119`。
- Sim pose publisher：`src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:75`。
- Start/pause/reset services：`src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:83` 到 `src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:101`。
- Replay modes：`src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:179` 到 `src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:193`。
- RViz displays：`src/low_speed_av_simulation/rviz/roadnet_simulation.rviz:11` 到 `src/low_speed_av_simulation/rviz/roadnet_simulation.rviz:26`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-SIM-001 | P1 | Not Verified | 新 simulation 包未在 ROS2 中编译。 | CMake 链接或依赖可能阻塞使用。 | 真实环境执行 colcon。 | `colcon build --packages-select low_speed_av_simulation`。 |
| AUD5-SIM-002 | P3 | Pass | simulation services 使用 `/simulation/start|pause|reset`。 | 符合目标合同。 | 无。 | `ros2 service list`。 |
| AUD5-SIM-003 | P2 | Partial | 基础 waypoint 用单条 LINE_STRIP 连接全部 waypoint，跨 edge 可能出现视觉伪连线。 | RViz 判断路线时可能误解。 | 按 edge 拆分 marker。 | RViz 观察。 |
| AUD5-SIM-004 | P2 | Partial | launch 默认 roadnet 是 bringup sample，但 fixed pose 默认值来自当前 `roadnet_ad_package_20260610T012525Z` 首点。 | 未传 roadnet 参数时，pose 可能与 sample package 不一致。 | launch 默认 fixed pose 跟随 loaded package 首点，或文档要求传 roadnet。 | 不传参数 launch 后空 start 规划。 |
| AUD5-SIM-005 | P2 | Partial | `trajectory_replay` yaw 主要取轨迹 yaw，fallback 只在 yaw 非有限时计算相邻点。 | 若轨迹 yaw 合法但方向不适配倒车，显示可能不完全表达车体方向。 | 后续按 gear/direction 处理回放朝向。 | 倒车路径 RViz 检查。 |

## ROS2 Commands Run Or Skipped

Run:

- `uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z`

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 launch low_speed_av_simulation simulation_visualization.launch.py ...`
- `ros2 topic echo /simulation/roadnet_markers`
- `rviz2 -d ...`

## Remaining Uncertainty

RViz 实际显示、marker frame、transient_local 行为和 launch include 均未在 ROS2 中验证。

