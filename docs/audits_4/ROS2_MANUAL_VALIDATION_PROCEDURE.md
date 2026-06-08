# ROS2 全工程人工验证流程

## 1. 目标与适用范围

本文档用于人工验证当前 Low Speed Roadnet AD Package v1.1 规划/控制 ROS2 工程的完整可用性，覆盖以下模块，而不是仅覆盖 Yunle SCU 输出与 LQR 控制器：

- `src/low_speed_av_interfaces`：消息与服务接口。
- `src/low_speed_av_planning`：RoadnetLoader、全局规划、运动规划、速度规划、规划节点服务与发布。
- `src/low_speed_av_control`：控制器、车辆模型、限幅、平滑、安全停车、Yunle SCU 输出。
- `src/low_speed_av_bringup`：配置、启动文件、示例 AD Package。
- 根目录 `scripts/`、`docs/`、`reports/`：离线验证、ROS2 集成说明、审计报告。

状态：Partial。当前 Windows Codex 环境未检测到 ROS2/colcon，因此 ROS2 构建、启动、话题和服务命令只能作为人工验证流程记录，不能在本环境中声明通过。

## 2. 关键证据

- 工程包边界：`src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_bringup`。
- 规划节点参数与发布/服务入口：`src/low_speed_av_planning/src/planning_node.cpp:45`、`src/low_speed_av_planning/src/planning_node.cpp:48`、`src/low_speed_av_planning/src/planning_node.cpp:72`、`src/low_speed_av_planning/src/planning_node.cpp:81`、`src/low_speed_av_planning/src/planning_node.cpp:88`、`src/low_speed_av_planning/src/planning_node.cpp:95`。
- 规划算法工厂：`src/low_speed_av_planning/src/global_planner_factory.cpp:12`、`src/low_speed_av_planning/src/global_planner_factory.cpp:15`、`src/low_speed_av_planning/src/reference_line_motion_planner.cpp:75`、`src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:23`。
- 控制节点话题、SCU 输出、LQR 配置与控制器选择：`src/low_speed_av_control/src/control_node.cpp:17`、`src/low_speed_av_control/src/control_node.cpp:24`、`src/low_speed_av_control/src/control_node.cpp:48`、`src/low_speed_av_control/src/control_node.cpp:68`、`src/low_speed_av_control/src/control_node.cpp:83`、`src/low_speed_av_control/src/control_node.cpp:121`。
- Bringup 默认配置：`src/low_speed_av_bringup/config/planning_params.yaml:16`、`src/low_speed_av_bringup/config/planning_params.yaml:24`、`src/low_speed_av_bringup/config/planning_params.yaml:31`、`src/low_speed_av_bringup/config/planning_params.yaml:39`、`src/low_speed_av_bringup/config/control_params.yaml:9`、`src/low_speed_av_bringup/config/control_params.yaml:15`、`src/low_speed_av_bringup/config/control_params.yaml:19`。
- Bringup 启动参数：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:31`、`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:39`。

## 3. 安全前提

在真实 ROS2 或实车环境执行本文档前，必须满足：

1. 首轮仅在仿真、台架、车轮离地或车辆动力禁用状态执行。
2. 实车联调必须有机械/电气急停、驾驶员或安全员、封闭低速场地。
3. 不允许在公共道路或未隔离区域直接执行运动命令。
4. 任何异常，包括无定位、无轨迹、超时、SCU 字段异常、规划失败、校验失败，都应按失败处理，并要求控制输出制动停车。
5. Yunle 底盘方向由挡位 D/R 选择，`scu_target_speed` 必须始终为非负 km/h，不允许用负速度表示倒车。

## 4. 环境预检

### 4.1 Windows Codex / 无 ROS2 环境

在当前 Windows Codex 环境中优先执行离线脚本。若系统 `python` 是 Windows Store 占位符，可使用用户允许的 `uv` Python：

```powershell
uv run python --version
uv run python scripts\validate_expected_tree.py
uv run --with pyyaml python scripts\validate_sample_ad_package.py
uv run --with pyyaml python scripts\offline_algorithm_smoke.py
uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py
uv run python scripts\offline_scu_lqr_smoke.py
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check_ros2_env.ps1
```

当前环境记录：

- `python` 指向 WindowsApps 占位符，不能直接执行项目脚本。
- `py` 未检测到。
- `uv run python --version` 可用。
- 上述 5 个离线脚本已可通过 `uv` 执行。
- `check_ros2_env.ps1` 报告 `SKIPPED_ROS2_UNAVAILABLE: colcon not found` 与 `SKIPPED_ROS2_UNAVAILABLE: ros2 not found`。

### 4.2 ROS2 人工验证环境

建议使用 Ubuntu + ROS2 Humble/Iron/Jazzy 或项目目标发行版。验证前确认：

```bash
echo $ROS_DISTRO
which ros2
which colcon
ros2 pkg prefix rclcpp
ros2 pkg prefix chassis_interfaces
```

人工确认：

- `chassis_interfaces` 已在同一工作区或已安装环境中可见。
- 当前工程位于 ROS2 workspace 的 `src` 下，或 workspace 能通过 `colcon` 发现四个包。
- 没有使用 ROS1 `catkin`、`roscpp` 作为构建依赖。

## 5. 离线验证项目

这些检查不依赖 ROS2 graph，适合在 Windows Codex、CI 或开发机上先执行。

| ID | 验证项 | 命令 | 期望结果 |
|---|---|---|---|
| OFF-01 | 工程树完整性 | `uv run python scripts\validate_expected_tree.py` | 四个包、接口、配置、脚本、文档、报告存在 |
| OFF-02 | 示例 AD Package v1.1 | `uv run --with pyyaml python scripts\validate_sample_ad_package.py` | canonical 路径存在，YAML waypoints、validation report、checksums 合法 |
| OFF-03 | 规划/控制算法冒烟 | `uv run --with pyyaml python scripts\offline_algorithm_smoke.py` | N0001 到 N0003 路由、轨迹拼接、Pure Pursuit/Stanley 有限输出 |
| OFF-04 | 剩余修复验证 | `uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py` | checksum 负例、语义 speed-zone/no-go、限幅/平滑、安全策略覆盖 |
| OFF-05 | SCU/LQR 冒烟 | `uv run python scripts\offline_scu_lqr_smoke.py` | SCU 挡位/单位/制动映射、LQR 有限输出与配置响应 |
| OFF-06 | ROS2 可用性提示 | `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\check_ros2_env.ps1` | 无 ROS2 时输出 `SKIPPED_ROS2_UNAVAILABLE`，有 ROS2 时列出后续命令 |

失败处理：

- 若 PyYAML 缺失，优先使用 `uv run --with pyyaml ...`。
- 若 `uv` 不可用，可使用 conda 环境 Python 或已知 Python 解释器，但必须记录实际命令与版本。
- 离线脚本失败时，不应进入实车验证。

## 6. 工程结构与构建验证

### 6.1 包结构

在 ROS2 workspace 根目录执行：

```bash
find src -maxdepth 2 -name package.xml -print
colcon list
```

人工确认：

- `low_speed_av_interfaces` 只包含 msg/srv/action 与接口构建。
- `low_speed_av_planning` 不依赖控制实现细节，负责 roadnet、route、trajectory。
- `low_speed_av_control` 不加载 roadnet，不做全局规划，负责 trajectory tracking 与命令输出。
- `low_speed_av_bringup` 只放 launch/config/sample data，不承载核心算法。

### 6.2 ROS2 构建

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

人工确认：

- `low_speed_av_control` 能找到 `chassis_interfaces`。
- 构建日志没有 `catkin`、`roscpp` 或 ROS1 adapter 依赖错误。
- 接口生成成功。
- launch/config/sample AD Package 被安装到 package share。
- 测试失败时记录具体包、测试名、失败日志，不允许笼统标记通过。

本 Windows Codex 环境：`colcon build`、`colcon test`、`colcon test-result` 均列入 `SKIPPED_ROS2_UNAVAILABLE`。

## 7. 接口验证

执行：

```bash
ros2 interface show low_speed_av_interfaces/msg/TrajectoryPoint
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show low_speed_av_interfaces/msg/GlobalRoute
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
ros2 interface show low_speed_av_interfaces/msg/VehicleState
ros2 interface show low_speed_av_interfaces/msg/ModuleStatus
ros2 interface show low_speed_av_interfaces/msg/RoadnetStatus
ros2 interface show low_speed_av_interfaces/srv/ReloadRoadnet
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/srv/SetPlannerAlgorithm
ros2 interface show low_speed_av_interfaces/srv/SetControllerAlgorithm
ros2 interface show chassis_interfaces/msg/ScuControlCommand
```

人工确认：

- `ControlCommand` 保留内部 SI 单位字段，并包含前/后轮转角。
- `ScuControlCommand` 包含 `scu_shift_level_request`、`scu_steering_angle_front`、`scu_steering_angle_rear`、`scu_target_speed`、`scu_brake_enable` 等字段。
- `ScuControlCommand` 不包含 `scu_drive_mode_request`；该字段只允许在文档中作为“ROS 消息未暴露、驱动内部固定处理”的说明出现。
- `PlanRoute`、`ReloadRoadnet`、`SetPlannerAlgorithm`、`SetControllerAlgorithm` 字段与后续服务调用命令一致。若字段名和本文命令不同，以 `ros2 interface show` 输出为准修正人工命令。

## 8. AD Package v1.1 验证

### 8.1 文件合同

验证示例包或目标包包含 canonical 路径：

```bash
ls src/low_speed_av_bringup/sample_ad_package
find src/low_speed_av_bringup/sample_ad_package -maxdepth 3 -type f | sort
```

必须存在：

- `project_manifest.json`
- `checksums.sha256`
- `map/map_metadata.yaml`
- `roadnet/topology.json`
- `roadnet/roadnet.json`
- `roadnet/route_graph.yaml`
- `trajectory/waypoints.yaml`
- `trajectory/waypoint_index.json`
- `semantics/areas.json`
- `semantics/task_points.json`
- `semantics/parking_points.json`
- `semantics/charging_points.json`
- `validation/validation_report.json`

人工确认：

- 不以 `manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json` 作为 primary path。
- `project_manifest.json` 中 `schema == "low_speed_roadnet_ad_package"`。
- `schema_version` 为 `1.1.0` 或兼容的 `1.1.x`。
- `validation.status != "failed"` 且 `validation.blocking_errors == 0`。
- `manifest.files` 路径能解析到 canonical 文件。

### 8.2 Loader 负例验证

在临时目录复制 sample AD Package，分别构造：

1. 修改 `validation/validation_report.json` 为 failed。
2. 修改 `blocking_errors` 为大于 0。
3. 修改 `trajectory/waypoint_index.json` 的索引越界。
4. 修改 `trajectory/waypoints.yaml` 后保持旧 checksum，制造 checksum mismatch。
5. 删除 required canonical 文件。

人工确认：

- RoadnetLoader 拒绝加载，并在 `/planning/roadnet_status` 或日志中给出明确文件路径和原因。
- checksum mismatch 在 `roadnet.verify_checksums=true` 时必须拒绝，而不是 warning-only。
- `end_index_exclusive` 被优先使用；仅缺失时兼容 legacy inclusive `end_index`。
- waypoint 字段映射正确：`kappa -> kappa_1pm`，`v_mps -> target_speed_mps`。

## 9. Bringup 与启动验证

### 9.1 默认启动

```bash
source install/setup.bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
```

另开终端：

```bash
source install/setup.bash
ros2 node list
ros2 topic list
ros2 service list
```

人工确认：

- 存在规划节点与控制节点，例如 `/low_speed_av_planning`、`/low_speed_av_control`。
- 存在 `/planning/global_route`、`/planning/trajectory`、`/planning/status`、`/planning/roadnet_status`。
- 存在 `/control/status`。
- Yunle 底盘输出默认存在 `/yunle_chassis/control/scu_control_command`，消息类型为 `chassis_interfaces/msg/ScuControlCommand`。
- 存在规划服务 `/low_speed_av_planning/reload_roadnet`、`/low_speed_av_planning/plan_route`、`/low_speed_av_planning/set_planner_algorithm`。
- 存在控制服务 `/low_speed_av_control/set_controller_algorithm`。

### 9.2 启动参数覆盖

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/absolute/path/to/ad_package \
  planning_params:=/absolute/path/to/planning_params.yaml \
  control_params:=/absolute/path/to/control_params.yaml
```

人工确认：

- 覆盖后的 `roadnet.package_path` 生效。
- 覆盖后的规划/控制参数生效。
- 安装后使用 package share 路径，不依赖源码目录绝对路径。

## 10. 参数验证

执行：

```bash
ros2 param get /low_speed_av_planning roadnet.package_path
ros2 param get /low_speed_av_planning roadnet.verify_checksums
ros2 param get /low_speed_av_planning topics.localization_pose_topic
ros2 param get /low_speed_av_planning topics.trajectory_topic
ros2 param get /low_speed_av_planning topics.global_route_topic
ros2 param get /low_speed_av_planning topics.planning_status_topic
ros2 param get /low_speed_av_planning global_planner.algorithm
ros2 param get /low_speed_av_planning motion_planner.algorithm
ros2 param get /low_speed_av_planning speed_planner.algorithm

ros2 param get /low_speed_av_control topics.localization_pose_topic
ros2 param get /low_speed_av_control topics.trajectory_topic
ros2 param get /low_speed_av_control topics.vehicle_state_topic
ros2 param get /low_speed_av_control topics.safety_status_topic
ros2 param get /low_speed_av_control topics.control_command_topic
ros2 param get /low_speed_av_control topics.scu_command_topic
ros2 param get /low_speed_av_control output.mode
ros2 param get /low_speed_av_control controller.algorithm
ros2 param get /low_speed_av_control vehicle.model
ros2 param get /low_speed_av_control safety.estop_latched
ros2 param get /low_speed_av_control lqr.q_lateral_error
ros2 param get /low_speed_av_control lqr.q_heading_error
ros2 param get /low_speed_av_control lqr.r_steering
ros2 param get /low_speed_av_control scu.max_steering_angle_deg
ros2 param get /low_speed_av_control scu.max_target_speed_kmh
```

人工确认：

- `/localization/pose` 是默认定位话题，并且可通过 YAML/launch 修改。
- 规划默认算法应与配置一致：global `astar`、motion `reference_line`、speed `curvature`。
- 控制默认算法以 bringup 配置为准；当前默认配置期望可设置为 `lqr`，节点源码声明默认值可能是 `pure_pursuit`，人工验证应以 launch 后实际参数为准。
- `output.mode` 为 `scu_control_command` 或 `both` 时，SCU topic 必须发布。
- `vehicle.model` 支持 `front_ackermann` 与 `dual_ackermann`。

## 11. 规划节点验证

### 11.1 Roadnet 加载

先查看服务字段：

```bash
ros2 interface show low_speed_av_interfaces/srv/ReloadRoadnet
```

按实际字段调用，示例：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/absolute/path/to/src/low_speed_av_bringup/sample_ad_package'}"

ros2 topic echo /planning/roadnet_status --once
ros2 topic echo /planning/status --once
```

人工确认：

- 加载成功时状态包含节点、边、waypoint、语义对象数量。
- 加载失败时规划节点进入不可规划状态，并发布失败原因。
- failed validation、blocking errors、checksum mismatch、bad waypoint index 不允许加载成功。

### 11.2 全局规划 Dijkstra/A*

查看服务字段：

```bash
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/srv/SetPlannerAlgorithm
```

切换算法并规划：

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'dijkstra', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003'}"

ros2 topic echo /planning/global_route --once
ros2 topic echo /planning/trajectory --once
ros2 topic echo /planning/status --once

ros2 service call /low_speed_av_planning/set_planner_algorithm \
  low_speed_av_interfaces/srv/SetPlannerAlgorithm \
  "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"

ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003'}"
```

人工确认：

- Dijkstra 与 A* 均能返回合法 route。
- `GlobalRoute` 包含 edge ids、node ids、length、estimated time。
- `Trajectory` 点序列连续、无重复拼接关节点、`s_m` 单调、字段有限。
- 当目标不可达或 roadnet 未加载时，规划发布 failure status 和安全 stop trajectory。
- `blocked_edges`、`allow_reverse`、edge cost 被纳入路由行为。

### 11.3 运动规划器

依次切换：

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'stop_and_wait', speed_planner_algorithm: 'constant'}"
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'frenet_lite', speed_planner_algorithm: 'constant'}"
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'hybrid_astar_parking', speed_planner_algorithm: 'constant'}"
```

人工确认：

- `reference_line` 输出可跟踪轨迹。
- `stop_and_wait` 输出显式停车轨迹，不输出高速运动轨迹。
- `frenet_lite`、`hybrid_astar_parking` 若仍为骨架，应明确 fallback/TODO，并保持安全低速或停车行为。
- 任一 skeleton 算法不得在未实现时输出不受控高速轨迹。

### 11.4 速度规划器

依次验证：

```bash
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'constant'}"
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'curvature'}"
ros2 service call /low_speed_av_planning/set_planner_algorithm low_speed_av_interfaces/srv/SetPlannerAlgorithm "{global_planner_algorithm: 'astar', motion_planner_algorithm: 'reference_line', speed_planner_algorithm: 'obstacle_aware'}"
```

人工确认：

- `constant` 使用保守默认速度。
- `curvature` 在曲率较大处降低速度。
- `obstacle_aware` 在障碍物接近时停车或显著降速；若障碍物输入仍为简化逻辑，应在状态/文档中明确限制。

## 12. 语义约束验证

构造或使用包含以下区域的 AD Package：

- `speed_zone`：覆盖部分 waypoint。
- `no_go_area` 或 `keepout`：覆盖某条 edge 或参考点。

验证步骤：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet low_speed_av_interfaces/srv/ReloadRoadnet "{package_path: '/absolute/path/to/semantic_test_package'}"
ros2 service call /low_speed_av_planning/plan_route low_speed_av_interfaces/srv/PlanRoute "{start_node_id: 'N0001', goal_node_id: 'N0003'}"
ros2 topic echo /planning/trajectory --once
ros2 topic echo /planning/status --once
```

人工确认：

- `speed_zone` 内轨迹点目标速度降低。
- `no_go_area`/`keepout` 覆盖的 edge 被阻断、绕行或规划失败。
- 如果无法绕行，规划失败状态明确说明 no-go/keepout 约束导致不可达，并发布 stop trajectory。
- 语义对象计数在 roadnet status 中可见或日志可见。

## 13. 控制节点验证

### 13.1 基础输入输出

查看接口：

```bash
ros2 interface show geometry_msgs/msg/PoseStamped
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show low_speed_av_interfaces/msg/VehicleState
ros2 interface show low_speed_av_interfaces/msg/ModuleStatus
```

发布定位示例：

```bash
ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped \
  "{header: {frame_id: 'map'}, pose: {position: {x: 0.0, y: 0.0, z: 0.0}, orientation: {w: 1.0}}}" \
  --rate 20
```

轨迹输入可使用以下任一方式：

1. 通过规划节点 `PlanRoute` 产生 `/planning/trajectory`。
2. 使用项目后续提供的 test publisher。
3. 手工根据 `ros2 interface show low_speed_av_interfaces/msg/Trajectory` 构造最小有效轨迹。

人工确认：

- 有有效定位和有效轨迹时，控制节点发布 `/control/status`。
- `output.mode=internal` 或 `both` 时，发布 `/control/command`。
- `output.mode=scu_control_command` 或 `both` 时，发布 `/yunle_chassis/control/scu_control_command`。
- 无有效轨迹或轨迹为空时，输出制动停车。

### 13.2 控制器切换

查看服务字段：

```bash
ros2 interface show low_speed_av_interfaces/srv/SetControllerAlgorithm
```

依次切换：

```bash
ros2 service call /low_speed_av_control/set_controller_algorithm low_speed_av_interfaces/srv/SetControllerAlgorithm "{controller_algorithm: 'pure_pursuit'}"
ros2 service call /low_speed_av_control/set_controller_algorithm low_speed_av_interfaces/srv/SetControllerAlgorithm "{controller_algorithm: 'stanley'}"
ros2 service call /low_speed_av_control/set_controller_algorithm low_speed_av_interfaces/srv/SetControllerAlgorithm "{controller_algorithm: 'lqr'}"
ros2 service call /low_speed_av_control/set_controller_algorithm low_speed_av_interfaces/srv/SetControllerAlgorithm "{controller_algorithm: 'mpc_sampler'}"
```

人工确认：

- Pure Pursuit 与 Stanley 是真实实现，输出有限 steering/curvature。
- LQR 使用 Riccati/DARE 与曲率前馈，不回退到 Stanley。
- MPC sampler 是确定性轻量采样，不依赖重型求解器。
- 切换未知控制器名称时拒绝或保持当前安全状态，不输出异常命令。

### 13.3 车辆模型切换

通过配置或参数重启验证：

```bash
ros2 param get /low_speed_av_control vehicle.model
```

分别设置并启动：

- `vehicle.model=front_ackermann`
- `vehicle.model=dual_ackermann`

人工确认：

- `front_ackermann`：前轮转角有效，后轮转角为 0。
- `dual_ackermann`：前/后轮转角均有限，符合反相四轮转向公式。
- 车辆模型输出仍经过限幅、平滑、NaN/Inf guard，然后才映射到 SCU。
- 超过最大转角时被限制或安全置零，不发布危险值。

## 14. Yunle SCU 输出验证

### 14.1 话题与类型

```bash
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo /yunle_chassis/control/scu_control_command
```

人工确认：

- 默认 topic 精确为 `/yunle_chassis/control/scu_control_command`。
- 类型精确为 `chassis_interfaces/msg/ScuControlCommand`。
- `scu_shift_level_request` 只出现 `1`、`2`、`3`。
- 不出现 `scu_drive_mode_request` 字段。

### 14.2 正常映射

在有效定位和轨迹下确认：

- drive 前进：`scu_shift_level_request=1`。
- neutral：`scu_shift_level_request=2`。
- reverse 倒车：`scu_shift_level_request=3`，`scu_target_speed >= 0`。
- `target_speed_mps` 转换为 `scu_target_speed = abs(mps) * 3.6`。
- `front_steering_angle_rad`、`rear_steering_angle_rad` 转换为物理角度 deg，并应用符号配置。
- 正常跟踪时 `scu_brake_enable=false`。
- 默认固定字段符合配置：灯光为 0，`scu_torque_or_speed_mode=1`，`steering_angle_speed_valid=false`，`brake_force_command_valid=false`。

### 14.3 异常映射

通过测试输入或辅助 publisher 注入：

- 非法 gear。
- 非有限 speed。
- 超范围 speed。
- 非有限 steering。
- 超范围 steering。

人工确认：

- 非法 gear 转为安全停车，不发布非法 shift。
- speed 异常或超范围映射为 `0.0`，并有 warning。
- steering 异常或超范围映射为 `0.0`，并有 warning。
- emergency stop 或 timeout 时：`scu_brake_enable=true`、`scu_target_speed=0`、前/后 steering 为 0、shift 仍为合法值。

## 15. LQR 控制器验证

### 15.1 选择 LQR

```bash
ros2 param get /low_speed_av_control controller.algorithm
ros2 service call /low_speed_av_control/set_controller_algorithm \
  low_speed_av_interfaces/srv/SetControllerAlgorithm \
  "{controller_algorithm: 'lqr'}"
```

人工确认：

- 日志中不出现“fallback to Stanley”。
- 控制输出有限。
- 低速场景使用 `min_speed_mps` 保持数值稳定。

### 15.2 直线轨迹左右误差

构造沿 x 轴的直线轨迹：

- 车辆在轨迹左侧。
- 车辆在轨迹右侧。

人工确认：

- 两种横向误差产生相反方向的转角响应。
- 输出经过车辆模型后，front_ackermann 与 dual_ackermann 均能生成有限 SCU 前/后轮角度。

### 15.3 曲线轨迹前馈

构造非零 `kappa_1pm` 的曲线轨迹，车辆位姿与参考点零误差：

人工确认：

- LQR 输出包含曲率前馈，非零曲率下不应输出完全零转角。
- 修改 `lqr.q_lateral_error`、`lqr.q_heading_error`、`lqr.r_steering` 后，同一误差场景下输出响应发生可解释变化。
- `lqr.max_steering_angle_rad` 限幅生效。

## 16. 安全与超时验证

### 16.1 Estop

发布安全状态。具体字段以接口为准：

```bash
ros2 interface show low_speed_av_interfaces/msg/ModuleStatus
ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus "{...}" --once
ros2 topic echo /yunle_chassis/control/scu_control_command --once
```

人工确认：

- Estop 激活后覆盖所有正常控制输出。
- SCU 输出为 brake stop：`scu_brake_enable=true`、speed 0、steering 0、shift 合法。
- 若配置为 latch，必须按文档的 clear/reset 条件解除；若非 latch，安全状态必须持续发布才保持 estop。
- Estop 恢复后，必须重新满足定位和轨迹有效性，不能直接恢复旧命令。

### 16.2 定位/轨迹超时

操作：

1. 停止 `/localization/pose` 发布，等待超过 localization timeout。
2. 停止 `/planning/trajectory` 发布，等待超过 trajectory timeout。
3. 发布空 trajectory。

人工确认：

- 每种情况都输出 controlled stop。
- `/control/status` 包含 timeout 或 empty trajectory 原因。
- SCU brake stop 字段正确。

### 16.3 NaN/Inf 与限幅平滑

使用辅助测试节点或手工构造异常轨迹：

- NaN pose。
- NaN trajectory point。
- Inf speed。
- 极大曲率。

人工确认：

- NaN/Inf guard 阻止异常值进入 SCU 输出。
- limiter 限制速度、转角、加速度、转角速率。
- smoother 不引入非有限值。
- 异常输入导致停车或安全置零，而不是继续发布上一帧高速命令。

## 17. 真实底盘台架验证

仅在台架、车轮离地或车辆禁用动力状态执行：

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command
ros2 topic hz /yunle_chassis/control/scu_control_command
```

如有 CAN 工具，确认 chassis driver 发出对应 CAN 帧，例如项目约定的 `0x121`。

人工确认：

- chassis driver 接收 `/yunle_chassis/control/scu_control_command`。
- 驱动内部处理 `SCU_Drive_Mode_Request=1`，ROS 消息不提供该字段。
- D/R 行为由 `scu_shift_level_request` 控制，速度非负。
- brake command 能使底盘停止。
- 转角单位为物理 deg，方向符号与车辆实际方向一致；若相反，调整 `front_steer_sign`、`rear_steer_sign`。

## 18. 完整人工验收清单

| 模块 | ID | 验证项目 | 操作/命令 | 期望结果 | 人工确认 | Pass/Fail | 备注 |
|---|---|---|---|---|---|---|---|
| 工程 | VAL-001 | 四包存在 | `colcon list` | 发现 interfaces/planning/control/bringup |  |  |  |
| 工程 | VAL-002 | 包边界清晰 | 检查源码目录 | 规划/控制职责分离 |  |  |  |
| 构建 | VAL-003 | ROS2 构建 | `colcon build --symlink-install` | 无缺失依赖 |  |  |  |
| 构建 | VAL-004 | ROS2 测试 | `colcon test && colcon test-result --verbose` | 测试通过或失败可定位 |  |  |  |
| 接口 | VAL-005 | 内部控制接口 | `ros2 interface show low_speed_av_interfaces/msg/ControlCommand` | 前/后转角字段存在 |  |  |  |
| 接口 | VAL-006 | SCU 接口 | `ros2 interface show chassis_interfaces/msg/ScuControlCommand` | 字段匹配，无 drive mode 字段 |  |  |  |
| AD Package | VAL-007 | canonical 文件 | `find sample_ad_package -maxdepth 3 -type f` | canonical 路径完整 |  |  |  |
| AD Package | VAL-008 | 旧路径非 primary | 检查包与日志 | 不依赖 old manifest/waypoints/report |  |  |  |
| Loader | VAL-009 | 有效包加载 | `reload_roadnet` | 加载成功并发布 status |  |  |  |
| Loader | VAL-010 | failed validation 拒绝 | 加载负例包 | 明确拒绝 |  |  |  |
| Loader | VAL-011 | blocking_errors 拒绝 | 加载负例包 | 明确拒绝 |  |  |  |
| Loader | VAL-012 | checksum mismatch 拒绝 | 篡改 waypoints | 明确拒绝 |  |  |  |
| Loader | VAL-013 | waypoint index 越界拒绝 | 篡改 index | 明确拒绝 |  |  |  |
| Loader | VAL-014 | 字段映射 | 查看轨迹输出 | kappa/v_mps 映射正确 |  |  |  |
| 规划 | VAL-015 | Dijkstra | 切换并 `PlanRoute` | 生成 route/trajectory |  |  |  |
| 规划 | VAL-016 | A* | 切换并 `PlanRoute` | 生成 route/trajectory |  |  |  |
| 规划 | VAL-017 | 不可达失败 | 规划不可达目标 | failure status + stop trajectory |  |  |  |
| 运动规划 | VAL-018 | reference_line | 切换算法 | 连续轨迹 |  |  |  |
| 运动规划 | VAL-019 | stop_and_wait | 切换算法 | 显式停车轨迹 |  |  |  |
| 运动规划 | VAL-020 | skeleton 安全 | frenet/hybrid_astar | 不输出危险高速 |  |  |  |
| 速度规划 | VAL-021 | constant | 切换算法 | 保守恒速 |  |  |  |
| 速度规划 | VAL-022 | curvature | 切换算法 | 曲率大处降速 |  |  |  |
| 速度规划 | VAL-023 | obstacle_aware | 注入接近障碍 | 降速或停车 |  |  |  |
| 语义 | VAL-024 | speed_zone | 语义测试包 | 区域内速度降低 |  |  |  |
| 语义 | VAL-025 | no_go/keepout | 语义测试包 | 阻断/绕行/失败停车 |  |  |  |
| Bringup | VAL-026 | 默认启动 | `ros2 launch ...` | 节点、话题、服务存在 |  |  |  |
| Bringup | VAL-027 | launch 参数覆盖 | 指定 package/config | 参数生效 |  |  |  |
| 参数 | VAL-028 | 定位话题 | `ros2 param get ... localization_pose_topic` | 默认 `/localization/pose` 且可改 |  |  |  |
| 控制 | VAL-029 | Pure Pursuit | 切换控制器 | 有限输出 |  |  |  |
| 控制 | VAL-030 | Stanley | 切换控制器 | 有限输出 |  |  |  |
| 控制 | VAL-031 | LQR | 切换控制器 | DARE + 前馈，有限输出 |  |  |  |
| 控制 | VAL-032 | MPC sampler | 切换控制器 | 确定性输出 |  |  |  |
| 车辆模型 | VAL-033 | front_ackermann | 启动配置 | 后轮角为 0 |  |  |  |
| 车辆模型 | VAL-034 | dual_ackermann | 启动配置 | 前/后轮角有限 |  |  |  |
| 安全 | VAL-035 | Estop | 发布安全状态 | SCU brake stop |  |  |  |
| 安全 | VAL-036 | 定位超时 | 停止 pose | controlled stop |  |  |  |
| 安全 | VAL-037 | 轨迹超时 | 停止 trajectory | controlled stop |  |  |  |
| 安全 | VAL-038 | 空轨迹 | 发布空 trajectory | controlled stop |  |  |  |
| 安全 | VAL-039 | NaN/Inf guard | 注入异常 | 停车或置零，无异常 SCU |  |  |  |
| SCU | VAL-040 | Topic/type | `ros2 topic info` | SCU topic/type 精确 |  |  |  |
| SCU | VAL-041 | D gear | 正常前进 | shift=1，speed>=0 |  |  |  |
| SCU | VAL-042 | R gear | 倒车轨迹 | shift=3，speed>=0 |  |  |  |
| SCU | VAL-043 | N gear | 中立命令 | shift=2 |  |  |  |
| SCU | VAL-044 | 非法 gear | 注入非法 gear | 安全停车，无非法 shift |  |  |  |
| SCU | VAL-045 | 单位转换 | echo SCU | speed km/h，steering deg |  |  |  |
| SCU | VAL-046 | 超范围 speed | 注入过大速度 | speed=0 且 warning |  |  |  |
| SCU | VAL-047 | 超范围 steering | 注入过大转角 | steering=0 且 warning |  |  |  |
| LQR | VAL-048 | 左右误差符号 | 直线轨迹左右偏置 | 转角方向相反 |  |  |  |
| LQR | VAL-049 | 曲率前馈 | 曲线零误差 | 非零曲率产生前馈 |  |  |  |
| LQR | VAL-050 | Q/R 配置响应 | 修改 Q/R | 输出响应变化 |  |  |  |
| 实车台架 | VAL-051 | chassis driver 接收 | echo + driver 日志 | driver 收到 SCU topic |  |  |  |
| 实车台架 | VAL-052 | CAN 输出 | CAN 工具 | 对应控制帧发出 |  |  |  |
| 实车台架 | VAL-053 | brake stop | 触发安全停车 | 底盘停止 |  |  |  |

## 19. 发现项

### VAL-FIND-001

Severity：P1  
状态：Not Verified  
影响：当前 Windows Codex 环境无 ROS2，无法确认真实 rclcpp 节点、服务、topic、interface generation、launch 安装路径在目标 ROS2 发行版中全部通过。  
推荐修复/验证：在真实 ROS2 workspace 执行第 6 至第 18 章命令，并保存日志到 `reports/`。  
验证方法：`colcon build`、`colcon test`、`ros2 launch`、`ros2 topic echo`、`ros2 service call`。

### VAL-FIND-002

Severity：P1  
状态：Partial  
影响：离线脚本已覆盖核心逻辑，但不能替代 C++/ROS2 运行时 ABI、参数声明、QoS、消息序列化、launch 安装路径和 chassis_interfaces 集成验证。  
推荐修复/验证：增加真实 ROS2 环境 CI 或台架验证记录。  
验证方法：构建日志、topic/service 手工记录、SCU echo 截图或日志。

### VAL-FIND-003

Severity：P2  
状态：Partial  
影响：部分服务调用示例依赖 srv 字段名；若接口字段后续变化，本文示例命令需要以 `ros2 interface show` 为准更新。  
推荐修复/验证：在 ROS2 环境执行接口查看后，确认命令字段完全匹配。  
验证方法：将实际可运行命令记录到测试报告。

## 20. ROS2 命令跳过记录

当前 Windows Codex 环境未检测到 ROS2/colcon，以下命令属于 `SKIPPED_ROS2_UNAVAILABLE`，不得声明已通过：

```text
SKIPPED_ROS2_UNAVAILABLE: source /opt/ros/$ROS_DISTRO/setup.bash
SKIPPED_ROS2_UNAVAILABLE: rosdep install --from-paths src --ignore-src -r -y
SKIPPED_ROS2_UNAVAILABLE: colcon build --symlink-install
SKIPPED_ROS2_UNAVAILABLE: colcon test
SKIPPED_ROS2_UNAVAILABLE: colcon test-result --verbose
SKIPPED_ROS2_UNAVAILABLE: ros2 interface show ...
SKIPPED_ROS2_UNAVAILABLE: ros2 launch low_speed_av_bringup planning_control_demo.launch.py
SKIPPED_ROS2_UNAVAILABLE: ros2 node list
SKIPPED_ROS2_UNAVAILABLE: ros2 topic list
SKIPPED_ROS2_UNAVAILABLE: ros2 topic echo ...
SKIPPED_ROS2_UNAVAILABLE: ros2 service list
SKIPPED_ROS2_UNAVAILABLE: ros2 service call ...
SKIPPED_ROS2_UNAVAILABLE: ros2 param get ...
```

## 21. 下一步建议

1. 在真实 ROS2 + `chassis_interfaces` 环境执行第 6 至第 18 章。
2. 优先完成离线脚本全部通过后，再启动 ROS2 节点。
3. 先仿真或台架验证 SCU 输出，再进行低速实车验证。
4. 把每次人工验证的命令、输出、截图或日志保存到 `reports/ros2_manual_validation_YYYYMMDD.md`。
