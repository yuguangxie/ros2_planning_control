# Ubuntu ROS2 运行阶段验证报告

日期：2026-06-10

工作目录：`/home/xie/planning_control/ros2_planning_control/ros2_planning_control`

验证方式：只测试验证，不修改源码、接口、配置、launch 或 roadnet 数据。执行命令前已规避 conda Python 环境：清理 conda/anaconda 相关 `PATH`，并 unset `CONDA_PREFIX`、`CONDA_DEFAULT_ENV`、`CONDA_SHLVL`、`PYTHONHOME`、`PYTHONPATH`。

原始日志目录：`reports/runtime_validation_logs/`

## 1. 测试环境

| 项目 | 结果 |
|---|---|
| OS / Shell | Ubuntu / bash |
| ROS2 发行版 | `humble` |
| 当前工程目录 | `/home/xie/planning_control/ros2_planning_control/ros2_planning_control` |
| 初始 git 状态 | `?? src/yunle_chassis/` |
| 真实车辆连接 | 未连接；只做仿真/bench 级 ROS topic 验证 |
| 日志目录 | `reports/runtime_validation_logs/` |

`ros2 --version` 记录：

- source 前执行失败：`PackageNotFoundError: ros2cli`。
- source 后命令可运行，但 Humble CLI 报 `unrecognized arguments: --version`。因此该命令不适合作为 Humble 版本检查方式，ROS 发行版以 `$ROS_DISTRO=humble` 记录。

## 2. 环境与构建验证

执行命令：

```bash
pwd
git status --short
echo $ROS_DISTRO
ros2 --version
colcon version-check || true
source /opt/ros/$ROS_DISTRO/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
source install/setup.bash
colcon test
colcon test-result --verbose
```

输出摘要：

| 检查项 | 结果 |
|---|---|
| `pwd` | `/home/xie/planning_control/ros2_planning_control/ros2_planning_control` |
| `git status --short` | `?? src/yunle_chassis/` |
| `$ROS_DISTRO` | `humble` |
| `colcon version-check` | 可运行，部分 colcon 包提示有新版，不影响本次构建 |
| `rosdep install` | 通过：`#All required rosdeps installed successfully` |
| `colcon build --symlink-install` | 通过：7 packages finished |
| `source install/setup.bash` | 通过 |
| `colcon test` | 通过：7 packages finished |
| `colcon test-result --verbose` | `0 tests, 0 errors, 0 failures, 0 skipped` |

成功构建的包：

- `low_speed_av_interfaces`
- `chassis_interfaces`
- `chassis_driver`
- `low_speed_av_planning`
- `low_speed_av_control`
- `low_speed_av_simulation`
- `low_speed_av_bringup`

结论：

- `chassis_interfaces` 可用。
- 未发现 catkin、roscpp、ament 相关构建错误。
- 当前仓库没有实际测试用例被执行，`colcon test-result` 的通过含义是“无测试失败”，不是“有测试覆盖”。

## 3. 接口验证

执行命令：

```bash
ros2 interface show low_speed_av_interfaces/srv/PlanRoute
ros2 interface show low_speed_av_interfaces/srv/ReloadRoadnet
ros2 interface show low_speed_av_interfaces/msg/Trajectory
ros2 interface show low_speed_av_interfaces/msg/ControlCommand
ros2 interface show chassis_interfaces/msg/ScuControlCommand
rg -n "scu_drive_mode_request" src
```

结果：

| 接口 | 验证结果 |
|---|---|
| `PlanRoute` | request 保留 `start_node_id`、`goal_node_id`、`start_task_point_id`、`goal_task_point_id`、`goal_parking_point_id` |
| 显式 start/goal node | 支持 |
| 空起点 | 由 planning 代码和参数 `planning.use_current_pose_as_start` 支持，没有新增不兼容字段 |
| `ReloadRoadnet` | request 为 `package_path`，response 为 `success/package_id/message` |
| `Trajectory` | 包含 header、trajectory id、package id、planner、points、`emergency_stop`、status |
| `ControlCommand` | 内部控制命令为 SI 单位，含 speed、acceleration、steering、gear、enable、emergency_stop |
| `ScuControlCommand` | 字段与 mapper 使用一致，含 shift、前/后轮转角、目标速度、brake、灯光和有效位 |
| `scu_drive_mode_request` | msg 中不存在；仅 README 说明 driver 内部固定 drive mode |

SCU shift 常量：

```text
SHIFT_LEVEL_D=1
SHIFT_LEVEL_N=2
SHIFT_LEVEL_R=3
```

## 4. Roadnet 包验证

执行命令：

```bash
find . -type d -name "roadnet_ad_package_20260610T012525Z"
ls roadnet_ad_package_20260610T012525Z
cat roadnet_ad_package_20260610T012525Z/project_manifest.json
cat roadnet_ad_package_20260610T012525Z/validation/validation_report.json
python3 scripts/validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
```

结果：

| 项目 | 结果 |
|---|---|
| 路网目录 | `./roadnet_ad_package_20260610T012525Z` |
| package id | `pkg_fc93b922-8948-4e0f-ba4b-e6cc66b08c4a_20260610T012525Z` |
| schema | `low_speed_roadnet_ad_package` |
| schema version | `1.1.0` |
| global frame | `map` |
| node 数量 | 16 |
| edge 数量 | 22 |
| waypoint 数量 | 496 |
| task point IDs | `RP-001` 到 `RP-007` |
| parking point IDs | 无 |
| charging point IDs | 无 |
| validation status | `warning` |
| blocking errors | 0 |
| warnings | 32 |
| 离线脚本 | 通过：`AD Package OK ... (16 nodes, 22 edges, 496 waypoints)` |

warning 类型：

- `HIGH_CURVATURE`
- `CURVATURE_CONTINUITY`
- `TOPOLOGY_EDGE_HIGH_CURVATURE`
- `WAYPOINT_CURVATURE_EXCEEDS_CONSTRAINT`

结论：

- 包结构和关键文件可用。
- 没有 blocking error。
- 存在高曲率和曲率连续性 warning，实车前必须保守处理。

## 5. 仿真可视化验证

启动命令：

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/home/xie/planning_control/ros2_planning_control/ros2_planning_control/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  rviz:=true
```

输出摘要：

- `/roadnet_visualization_node` 启动。
- `/sim_localization_pose_publisher` 启动。
- `/rviz2` 启动。
- RViz 输出 `OpenGl version: 4.6`。
- roadnet visualization node 输出加载成功：16 nodes、22 edges、496 waypoints。
- sim pose publisher 输出加载成功：496 roadnet replay waypoints。

Topic/service 验证：

| 检查项 | 结果 |
|---|---|
| `/localization/pose` | 持续发布，约 20 Hz |
| `frame_id` | `map` |
| quaternion | 有效，范数 1.0 |
| `/simulation/roadnet_markers` | 非空，12 markers |
| `/simulation/vehicle_markers` | 非空，1 marker |
| `/simulation/route_markers` | 规划后非空，3 markers |
| `/simulation/trajectory_path` | 规划后非空，67 poses，frame `map` |
| `/simulation/pause` | `success=True` |
| `/simulation/start` | `success=True` |
| `/simulation/reset` | `success=True` |

RViz 结论：

- RViz 进程可启动。
- 本次没有人工截图/目视确认画面，因此“路网、车辆、语义点、路线和轨迹是否肉眼可见”未做人工确认。
- 已用 topic、MarkerArray、Path 数据完成 headless 验证。

## 6. Planning / Control Bringup 验证

启动命令：

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  roadnet_package_path:=/home/xie/planning_control/ros2_planning_control/ros2_planning_control/roadnet_ad_package_20260610T012525Z
```

结果：

| 检查项 | 结果 |
|---|---|
| planning node | `/low_speed_av_planning` 存在 |
| control node | `/low_speed_av_control` 存在 |
| PlanRoute service | `/low_speed_av_planning/plan_route` 存在 |
| ReloadRoadnet service | `/low_speed_av_planning/reload_roadnet` 存在 |
| `/planning/roadnet_status` | 可发布；提前 echo 后 reload 可捕获 ready |
| `roadnet.package_path` | 正确绝对路径 |
| `planning.use_current_pose_as_start` | `True` |
| `controller.algorithm` | `lqr` |
| `output.mode` | `scu_control_command` |

注意：

- late `ros2 topic echo --once /planning/roadnet_status` 没有捕获到消息。
- 先启动 echo，再调用 reload，可以捕获：

```text
ready: true
nodes: 16
edges: 22
waypoints: 496
message: roadnet ready
```

## 7. ReloadRoadnet 验证

执行命令：

```bash
ros2 service call /low_speed_av_planning/reload_roadnet \
  low_speed_av_interfaces/srv/ReloadRoadnet \
  "{package_path: '/home/xie/planning_control/ros2_planning_control/ros2_planning_control/roadnet_ad_package_20260610T012525Z'}"
```

返回：

```text
success=True
package_id='pkg_fc93b922-8948-4e0f-ba4b-e6cc66b08c4a_20260610T012525Z'
message='roadnet ready'
```

结论：ReloadRoadnet 服务可用。

## 8. 路网节点规划验证

| 用例 | 命令摘要 | 结果 | route | trajectory |
|---|---|---|---|---|
| 相邻节点 | `N0001 -> N0002` | 成功 | `N0001,N0002`；`E_C-001_F` | 11 点，速度 1.0 m/s，gear 1 |
| 多 edge | `N0001 -> N0005` | 成功 | `N0001,N0002,N0003,N0004,N0005`；4 条 edge | 67 点，速度 1.0 m/s，gear 1 |
| 不存在节点 | `N0001 -> BAD_NODE` | 按预期失败 | `route_failed`，空 route | `failure_stop`，1 点，速度 0，`emergency_stop=true` |

无效节点失败信息：

```text
start or goal node is not in topology
```

结论：

- node-id 规划成功路径可用。
- 不存在节点失败路径安全，信息清晰。

## 9. 任务点规划验证

真实 task point ID：

```text
RP-001, RP-002, RP-003, RP-004, RP-005, RP-006, RP-007
```

| 用例 | 命令摘要 | 结果 | 现象 |
|---|---|---|---|
| 当前定位/空 start -> task point | `goal_task_point_id: RP-001` | 失败 | `success=False`，`start or goal node is not in topology` |
| task point -> task point | `start_task_point_id: RP-003`, `goal_task_point_id: RP-001` | 失败 | 同上 |
| 无效 task point | `goal_task_point_id: BAD_TASK` | 按预期失败 | `start or goal node cannot be resolved` |

失败时输出：

- `/planning/status` 为 `failure`。
- `/planning/trajectory` 为 `failure_stop`。
- `emergency_stop=true`。
- 轨迹速度为 0。

疑似原因：

- 当前 `task_points.json` 中 task point 的 `linked_node_id` 为 `null`，但 `linked_edge_id` 有值。
- 只读检查显示 `RoadnetLoader::yaml_string()` 对存在的 YAML key 直接 `as<std::string>()`。
- YAML null 可能被解析成非空字符串，例如 `"null"`。
- `PlanningNode::resolve_goal_node()` 优先使用 `linked_node_id`，因此可能返回无效 node id，未进入 `linked_edge_id` fallback。

结论：任务点规划成功路径当前不可用。

## 10. 停车点规划验证

当前文件：

```text
roadnet_ad_package_20260610T012525Z/semantics/parking_points.json
```

内容摘要：

```text
parking_points: []
```

| 用例 | 结果 |
|---|---|
| 当前定位/空 start -> 真实 parking point | 无法测试，当前包没有 parking point |
| 显式 start_node_id -> 真实 parking point | 无法测试，当前包没有 parking point |
| 无效 parking point | 按预期失败，发布安全停车轨迹 |

结论：

- 停车点失败路径安全。
- 停车点成功路径被数据包阻塞，标记为 `BLOCKED_NO_PARKING_POINTS_IN_PACKAGE`。

## 11. 当前定位作为规划起点验证

执行命令：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

结果：

| 检查项 | 结果 |
|---|---|
| service | `success=True`, `message='ok'` |
| route | `N0001 -> N0002 -> N0003` |
| edges | `E_C-001_F`, `E_L-001_F` |
| trajectory | 30 点 |
| 当前 pose 与轨迹起点距离 | 0.0 m |
| status 诊断 | 包含 `matched current pose to waypoint=WP_E_C-001_F_000000 edge=E_C-001_F start_node=N0001 ...` |

结论：

- 当前定位作为规划起点可用。
- task point 目标因第 9 节问题失败。
- parking point 目标因当前包无 parking point 未能验证成功路径。

## 12. 控制与 SCU 输出验证

执行命令：

```bash
ros2 topic echo --once /control/status
ros2 topic info /yunle_chassis/control/scu_control_command
ros2 topic echo --once /yunle_chassis/control/scu_control_command
ros2 topic hz /yunle_chassis/control/scu_control_command
```

Topic 验证：

| 检查项 | 结果 |
|---|---|
| SCU topic | `/yunle_chassis/control/scu_control_command` |
| SCU type | `chassis_interfaces/msg/ScuControlCommand` |
| Publisher count | 1 |
| Subscription count | 0 |
| 频率 | 约 50 Hz |

安全停车样本：

```text
scu_shift_level_request: 1
scu_steering_angle_front: 0.0
scu_steering_angle_rear: 0.0
scu_target_speed: 0.0
scu_brake_enable: true
```

前进规划后 SCU 窗口：

| 检查项 | 结果 |
|---|---|
| 非零 SCU 样本 | 有，25 个 |
| 最大速度 | 约 3.6 km/h |
| 速度符号 | 非负 |
| shift | 1，即 D 档 |
| steering | 前轮约 -5.23 deg，后轮 0 |
| brake | 运动样本为 false，timeout 后为 true |

倒车 edge 验证：

| 检查项 | 结果 |
|---|---|
| 路线 | `N0015 -> N0014` |
| edge | `E_C-007_R` |
| trajectory gear | 2 |
| SCU shift | 3 出现在倒车运动窗口 |
| speed | 非负 km/h |
| invalid shift | 未出现 |

控制持续性观察：

- control 确实收到规划轨迹，并短时间发布非零 SCU 命令。
- 随后 `/control/status` 进入 `stopping`，message 为 `trajectory_timeout`。
- 原因是 planning service 只发布一次 trajectory，而 control 参数 `controller.trajectory_timeout_s` 为 0.5 秒。

结论：

- SCU 字段、topic、D/R shift、速度单位、非负速度、安全停车输出通过验证。
- 规划结果进入控制模块后的“瞬时输出”通过。
- 持续跟踪只部分可用，需要修复或明确设计。

## 13. 安全与失败路径验证

验证用例：

| 用例 | 结果 |
|---|---|
| pause simulation 后空起点规划 | 失败信息清晰：`current localization pose is stale: age=3.44411s timeout=1s` |
| stale pose 失败轨迹 | `failure_stop`，速度 0，`emergency_stop=true` |
| 无效 goal node | 失败信息清晰，安全停车 |
| 无效 task point | 失败信息清晰，安全停车 |
| 无效 parking point | 失败信息清晰，安全停车 |
| safety estop | control 进入 `safety_estop` |
| estop 下 SCU | speed=0，brake=true，shift 合法 |

estop 验证摘要：

```text
states=['safety_estop']
messages=['manual estop']
scu_target_speed=0.0
scu_brake_enable=true
scu_shift_level_request=1
```

结论：安全失败路径整体通过。

## 14. 问题分级

| ID | 等级 | 问题 | 复现命令 | 日志摘要 | 影响 | 建议修复方向 |
|---|---|---|---|---|---|---|
| UVR-P1-001 | P1 | 真实 task point 规划不可用 | `PlanRoute` 使用 `goal_task_point_id: RP-001` 或 `start_task_point_id: RP-003` | `success=False`，`start or goal node is not in topology`，发布 `failure_stop` | 任务点任务无法执行 | YAML null 按空值处理；确保 `linked_edge_id` fallback 生效；增加 ROS2 回归测试 |
| UVR-P1-002 | P1 | 控制持续跟踪只部分可用 | 规划 `N0001 -> N0005` 后连续采集 `/control/status` 和 SCU | SCU 短暂非零，随后 `trajectory_timeout` 安全停车 | demo 无法持续执行完整路线 | 明确 trajectory 生命周期：周期发布、controller hold，或调整 timeout 合同 |
| UVR-P2-001 | P2 | parking point 成功路径无法验证 | 查看 `semantics/parking_points.json` | `parking_points: []` | 停车功能 readiness 未知 | 准备含真实 parking point 的测试包并重跑 |
| UVR-P2-002 | P2 | Roadnet 有 32 个非阻塞 warning | 查看 `validation/validation_report.json` | 高曲率、曲率连续性、waypoint 曲率 warning | 实车跟踪风险升高 | 平滑路径或降低速度限制 |
| UVR-P3-001 | P3 | `ros2 --version` 不适合当前 Humble CLI | 执行 `ros2 --version` | source 后仍报 `unrecognized arguments: --version` | 自动验证命令有噪声失败 | 使用 `$ROS_DISTRO`、`ros2 doctor` 或包版本命令 |
| UVR-P3-002 | P3 | `/planning/roadnet_status` 晚订阅容易错过 | reload 后再 `echo --once` | late echo 无样本；pre-echo 后 reload 可捕获 | 人工验证易误判 | 使用 transient local QoS 或周期发布状态 |

未发现 P0 问题。

## 15. 是否可以进入台架验证

结论：可以进入更严格的 bench-only / wheels-off / message-monitor 级验证，但不建议连接真实底盘做运动测试。

可以继续的验证：

- ROS topic/type/frequency 监控；
- SCU 命令字段台架检查；
- estop 和 timeout 安全停车检查；
- wheels-off 模式下低速闭环观察。

暂不建议真实车辆运动测试的原因：

- 任务点规划成功路径失败；
- 停车点成功路径未验证；
- 控制持续跟踪仅部分可用；
- roadnet 仍有高曲率和曲率连续性 warning；
- 未完成真实 chassis driver 订阅与执行行为的台架安全闭环验证。
