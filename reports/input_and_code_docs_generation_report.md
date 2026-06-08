# 输入与代码说明文档生成报告

## 1. 目标

生成完整中文文档，说明当前 ROS2 规划/控制工程需要哪些输入、如何触发路线规划、规划和控制代码如何工作，以及操作员如何启动和验证系统。

本次为文档任务，仅新增文档和报告，未修改运行时代码。

## 2. 新增文件

- `docs/PLANNING_CONTROL_INPUT_GUIDE.md`
- `docs/ROUTE_PLANNING_OPERATION_GUIDE.md`
- `docs/PLANNING_MODULE_CODE_WALKTHROUGH.md`
- `docs/CONTROL_MODULE_CODE_WALKTHROUGH.md`
- `docs/TECHNICAL_ROUTE_AND_DATAFLOW.md`
- `docs/ROS2_COMMAND_EXAMPLES.md`
- `docs/OPERATOR_STARTUP_CHECKLIST.md`
- `docs/PLANNING_CONTROL_DOCS_INDEX.md`
- `reports/input_and_code_docs_generation_report.md`

## 3. 已检查的源文件

接口：

- `src/low_speed_av_interfaces/msg/TrajectoryPoint.msg`
- `src/low_speed_av_interfaces/msg/Trajectory.msg`
- `src/low_speed_av_interfaces/msg/GlobalRoute.msg`
- `src/low_speed_av_interfaces/msg/ControlCommand.msg`
- `src/low_speed_av_interfaces/msg/VehicleState.msg`
- `src/low_speed_av_interfaces/msg/ModuleStatus.msg`
- `src/low_speed_av_interfaces/msg/RoadnetStatus.msg`
- `src/low_speed_av_interfaces/srv/ReloadRoadnet.srv`
- `src/low_speed_av_interfaces/srv/PlanRoute.srv`
- `src/low_speed_av_interfaces/srv/SetPlannerAlgorithm.srv`
- `src/low_speed_av_interfaces/srv/SetControllerAlgorithm.srv`

规划：

- `src/low_speed_av_planning/src/planning_node.cpp`
- `src/low_speed_av_planning/src/roadnet_loader.cpp`
- `src/low_speed_av_planning/src/topology_graph.cpp`
- `src/low_speed_av_planning/src/dijkstra_planner.cpp`
- `src/low_speed_av_planning/src/astar_planner.cpp`
- `src/low_speed_av_planning/src/reference_line_motion_planner.cpp`
- `src/low_speed_av_planning/src/stop_and_wait_motion_planner.cpp`
- `src/low_speed_av_planning/src/curvature_speed_planner.cpp`
- `src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp`
- `src/low_speed_av_planning/include/low_speed_av_planning/roadnet_types.hpp`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_planning/launch/planning.launch.py`

控制：

- `src/low_speed_av_control/src/control_node.cpp`
- `src/low_speed_av_control/src/controller_factory.cpp`
- `src/low_speed_av_control/src/vehicle_model_factory.cpp`
- `src/low_speed_av_control/src/pure_pursuit_controller.cpp`
- `src/low_speed_av_control/src/stanley_controller.cpp`
- `src/low_speed_av_control/src/lqr_controller.cpp`
- `src/low_speed_av_control/src/mpc_sampler_controller.cpp`
- `src/low_speed_av_control/src/front_ackermann_model.cpp`
- `src/low_speed_av_control/src/dual_ackermann_model.cpp`
- `src/low_speed_av_control/src/command_limiter.cpp`
- `src/low_speed_av_control/src/command_smoother.cpp`
- `src/low_speed_av_control/src/scu_command_mapper.cpp`
- `src/low_speed_av_control/include/low_speed_av_control/control_types.hpp`
- `src/low_speed_av_control/config/control_params.yaml`
- `src/low_speed_av_control/launch/control.launch.py`

Bringup 与示例数据：

- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `src/low_speed_av_bringup/config/vehicle_params.yaml`
- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py`
- `src/low_speed_av_bringup/sample_ad_package/project_manifest.json`
- `src/low_speed_av_bringup/sample_ad_package/semantics/task_points.json`
- `src/low_speed_av_bringup/sample_ad_package/semantics/parking_points.json`

已有文档与审计：

- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/LQR_CONTROLLER_DESIGN.md`
- `docs/audits_3/`
- `docs/audits_4/`
- `reports/`

## 4. 确认到的实际服务

### ReloadRoadnet

服务名：

```text
/low_speed_av_planning/reload_roadnet
```

类型：

```text
low_speed_av_interfaces/srv/ReloadRoadnet
```

实际字段：

```text
string package_path
---
bool success
string package_id
string message
```

### PlanRoute

服务名：

```text
/low_speed_av_planning/plan_route
```

类型：

```text
low_speed_av_interfaces/srv/PlanRoute
```

实际字段：

```text
string start_node_id
string goal_node_id
string start_task_point_id
string goal_task_point_id
string goal_parking_point_id
---
bool success
string message
GlobalRoute route
```

### SetPlannerAlgorithm

服务名：

```text
/low_speed_av_planning/set_planner_algorithm
```

类型：

```text
low_speed_av_interfaces/srv/SetPlannerAlgorithm
```

实际字段：

```text
string global_planner_algorithm
string motion_planner_algorithm
string speed_planner_algorithm
---
bool success
string message
```

### SetControllerAlgorithm

服务名：

```text
/low_speed_av_control/set_controller_algorithm
```

类型：

```text
low_speed_av_interfaces/srv/SetControllerAlgorithm
```

实际字段：

```text
string controller_algorithm
string vehicle_model
---
bool success
string message
```

## 5. 确认到的核心 topics

| Topic | 类型 | 说明 |
|---|---|---|
| `/planning/global_route` | `low_speed_av_interfaces/msg/GlobalRoute` | 规划路线 |
| `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | 控制参考轨迹 |
| `/planning/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 规划状态 |
| `/planning/roadnet_status` | `low_speed_av_interfaces/msg/RoadnetStatus` | roadnet 加载状态 |
| `/localization/pose` | `geometry_msgs/msg/PoseStamped` | 控制定位输入 |
| `/vehicle/state` | `low_speed_av_interfaces/msg/VehicleState` | 车辆状态输入 |
| `/safety/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 安全状态输入 |
| `/control/command` | `low_speed_av_interfaces/msg/ControlCommand` | 内部/调试控制输出，仅 `internal` 或 `both` 模式 |
| `/control/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 控制状态 |
| `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | 最终 Yunle 底盘命令 |

## 6. 关键结论

1. 路线规划由 `/low_speed_av_planning/plan_route` 服务触发。
2. AD Package 默认由 bringup launch 注入 `roadnet.package_path`，也可通过 `ReloadRoadnet` 服务重载。
3. `PlanRoute` 可以使用 node ID，也可以使用 task point 或 parking point 作为目标解析入口。
4. 规划成功后发布 `/planning/global_route` 和 `/planning/trajectory`。
5. 控制节点需要 `/localization/pose` 和 `/planning/trajectory` 才能正常输出；缺失或超时会停车。
6. 当前默认底盘输出是 `/yunle_chassis/control/scu_control_command`，类型为 `chassis_interfaces/msg/ScuControlCommand`。
7. 控制器先输出内部 SI 单位 curvature/speed，车辆模型转前后轮转角，最后 SCU mapper 转 km/h 与 deg。
8. Estop、定位超时、轨迹超时、空轨迹、NaN/Inf guard 都应进入制动停车路径。

## 7. 不确定项与说明

- ROS2 命令未在当前 Windows Codex 环境执行，因此文档中的 `ros2 service call`、`ros2 topic echo`、`colcon build` 都是“真实 ROS2 环境中运行的命令示例”，不是本地已通过结果。
- `chassis_interfaces/msg/ScuControlCommand` 的接口内容依赖目标 ROS2 workspace 中的 `chassis_interfaces` 包；本次依据项目代码引用和既有文档说明生成。
- 手工发布完整 `Trajectory` 消息较长，文档建议优先通过 `PlanRoute` 生成 `/planning/trajectory`。

## 8. ROS2 命令跳过记录

当前环境未检测到 ROS2/colcon，以下命令未执行：

```text
SKIPPED_ROS2_UNAVAILABLE: source /opt/ros/$ROS_DISTRO/setup.bash
SKIPPED_ROS2_UNAVAILABLE: rosdep install --from-paths src --ignore-src -r -y
SKIPPED_ROS2_UNAVAILABLE: colcon build --symlink-install
SKIPPED_ROS2_UNAVAILABLE: colcon test
SKIPPED_ROS2_UNAVAILABLE: colcon test-result --verbose
SKIPPED_ROS2_UNAVAILABLE: ros2 launch low_speed_av_bringup planning_control_demo.launch.py
SKIPPED_ROS2_UNAVAILABLE: ros2 service call /low_speed_av_planning/plan_route ...
SKIPPED_ROS2_UNAVAILABLE: ros2 topic echo /planning/trajectory
SKIPPED_ROS2_UNAVAILABLE: ros2 topic echo /yunle_chassis/control/scu_control_command
```

## 9. 本地检查

使用 `uv` Python 在当前环境执行了非 ROS2 检查：

```text
uv run python scripts\validate_expected_tree.py
uv run --with pyyaml python scripts\validate_sample_ad_package.py
```

检查用于确认工程树和 sample AD Package 基本有效；不能替代 ROS2 构建和运行时验证。

## 10. 推荐下一步

在真实 ROS2 + `chassis_interfaces` 环境中按以下顺序验证：

1. 阅读 `docs/PLANNING_CONTROL_DOCS_INDEX.md`。
2. 执行 `docs/ROS2_COMMAND_EXAMPLES.md` 中的 build、launch、parameter、service、topic 命令。
3. 使用 `docs/OPERATOR_STARTUP_CHECKLIST.md` 做现场表格化验收。
4. 将实际输出保存为 `reports/ros2_manual_validation_<date>.md`。
