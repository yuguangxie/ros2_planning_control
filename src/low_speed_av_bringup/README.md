# low_speed_av_bringup

## 模块定位
`low_speed_av_bringup` 提供低速自动驾驶规划/控制系统的启动文件、默认配置和样例 Low Speed Roadnet AD Package v1.1。

该包不包含算法逻辑。它用于把以下模块组合起来：

- `low_speed_av_planning`
- `low_speed_av_control`
- `low_speed_av_interfaces`
- `low_speed_av_simulation`（仅在仿真 launch 中）

## 文件结构

```text
low_speed_av_bringup/
  CMakeLists.txt
  package.xml
  launch/
    planning_control_demo.launch.py
    planning_control_closed_loop_sim.launch.py
  config/
    planning_params.yaml
    control_params.yaml
    vehicle_params.yaml
    control_sim_params.yaml
  sample_ad_package/
    project_manifest.json
    checksums.sha256
    map/map_metadata.yaml
    roadnet/roadnet.json
    roadnet/topology.json
    roadnet/route_graph.yaml
    trajectory/waypoints.yaml
    trajectory/waypoint_index.json
    trajectory/waypoints.csv
    semantics/*.json
    validation/validation_report.json
```

## 样例 AD Package
样例包使用 v1.1 canonical paths：

```text
project_manifest.json
checksums.sha256
map/map_metadata.yaml
roadnet/topology.json
roadnet/route_graph.yaml
roadnet/roadnet.json
trajectory/waypoints.yaml
trajectory/waypoint_index.json
semantics/areas.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
```

旧路径不作为 primary input：

```text
manifest.json
trajectory/waypoints.json
validation_report.json
```

## 配置文件说明

### config/planning_params.yaml
规划模块参数：

- roadnet 加载策略。
- 全局规划算法。
- motion planner。
- speed planner。
- planning topics。

### config/control_params.yaml
控制模块参数：

- localization/trajectory/vehicle/safety topics。
- 控制器选择。
- 车辆模型。
- 速度、转角、加速度限制。
- smoother 参数。

### config/vehicle_params.yaml
样例车辆 profile：

- 支持 `front_ackermann` 和 `dual_ackermann`。
- 包含 wheel base、track width、车身尺寸。
- 包含速度、加速度、转角限制。

## Launch 说明
启动文件：

```text
launch/planning_control_demo.launch.py
```

启动内容：

- `low_speed_av_planning/planning_node`
- `low_speed_av_control/control_node`

参数：

- `planning_params`：规划参数 YAML 路径。
- `control_params`：控制参数 YAML 路径。

当前 launch 默认参数为空，适合集成系统显式传参。后续可以改为自动定位安装后的 config 和 sample package。

### Control closed-loop SIL

`planning_control_closed_loop_sim.launch.py` 只启动 Planning、Control、Simulation plant、Roadnet visualization 和可选 RViz。它不会启动 `chassis_driver_node`、`keyboard_scu_control_node` 或任何 UDP/CAN 节点。Simulation 专用 Control override 强制 `output.mode=internal`、`vehicle_state.required=true`，不会改变生产 Control 默认的 `output.mode=both`。

```bash
source /opt/ros/humble/setup.bash
source install/setup.bash
ros2 launch low_speed_av_bringup \
  planning_control_closed_loop_sim.launch.py \
  rviz:=true \
  controller_algorithm:=lqr \
  vehicle_model:=front_ackermann
```

可用参数：`roadnet_package_path`、`planning_params`、`control_params`、`simulation_params`、`controller_algorithm`、`vehicle_model`、`rviz`、`start_paused`。默认 bundled sample 在 `--symlink-install` 下只解析仓库可信 sample 的物理根；用户传入的 AD Package 仍由 Loader strict containment 验证。

请求 sample 路线：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

诊断：

```bash
ros2 topic hz /control/command
ros2 topic hz /localization/pose
ros2 topic echo /control/status
ros2 topic echo /simulation/status
ros2 topic echo /simulation/diagnostics
ros2 topic echo /vehicle/state
ros2 node list
```

RViz 中绿色为 Planning trajectory，粉色为 actual pose path；Roadnet、GlobalRoute/goal、vehicle marker 同时显示。SIL 结果不得解释为 HIL 或真实车辆性能。

## 推荐使用方式
在真实 ROS2 环境中：

```bash
source /opt/ros/<distro>/setup.bash
colcon build
source install/setup.bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py \
  planning_params:=/path/to/planning_params.yaml \
  control_params:=/path/to/control_params.yaml
```

当前 Codex 环境可能没有 ROS2，因此不要把 `colcon build` 或 `ros2 launch` 作为本地必跑检查。

## 无 ROS2 验证
可在仓库根目录运行：

```powershell
python scripts\validate_expected_tree.py
python scripts\validate_sample_ad_package.py
python scripts\offline_algorithm_smoke.py
```

如果 `python` 不可用，可使用已安装的其他 Python 解释器。

## 后续改进建议
- launch 默认自动加载安装后的 config。
- planning config 默认指向安装后的 sample package。
- 增加 demo mission service call 示例。
- 增加 ROS2 topic/service 验证脚本。

## Phase 14 integration test

`test/test_planning_control_safety_launch.py` 使用 `launch_testing` 启动 production Planning 与 Control，验证无效规划目标产生 emergency trajectory，并在 `/control/command` 与 SCU command 上得到 brake stop。所有等待都有超时，且不启动 Chassis 网络节点。

当前 Windows 环境没有 ROS2，测试状态为 `SKIPPED_ROS2_UNAVAILABLE`。Chassis publisher 停止后的 watchdog stop 规格仍以 `SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002` 保留。
