# 仿真可视化与当前定位起点规划集成流程

## 1. 端到端流程

```text
roadnet_ad_package_20260610T012525Z
  -> roadnet visualization node
  -> RViz markers
  -> simulated localization publisher
  -> /localization/pose
  -> planning node current-pose matcher
  -> PlanRoute goal request
  -> /planning/global_route
  -> /planning/trajectory
  -> control node
  -> /yunle_chassis/control/scu_control_command
```

## 2. 数据流表

| 阶段 | 输入 | 处理模块 | 输出 |
|---|---|---|---|
| 路网加载 | AD Package v1.1 目录 | `RoadnetLoader` | nodes、edges、waypoints、semantics |
| 基础可视化 | RoadnetPackage | `roadnet_visualization_node` | `/simulation/roadnet_markers` |
| 模拟定位 | fixed pose / trajectory / roadnet waypoints | `sim_localization_pose_publisher_node` | `/localization/pose` |
| 当前起点匹配 | `/localization/pose` + waypoints/index | `PlanningNode` | start node |
| 全局规划 | start node + goal node | Dijkstra/A* | `GlobalRoute` |
| 轨迹生成 | edge ids + waypoint index | reference_line motion planner | `Trajectory` |
| 速度规划 | trajectory + speed planner config | curvature/constant/obstacle_aware | 目标速度 |
| 控制跟踪 | `/localization/pose` + `/planning/trajectory` | control node | internal control command |
| 底盘映射 | internal command | `ScuCommandMapper` | `/yunle_chassis/control/scu_control_command` |

## 3. 推荐运行命令

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
source install/setup.bash
```

```bash
ros2 launch low_speed_av_simulation simulation_visualization.launch.py \
  roadnet_package_path:=/absolute/path/to/roadnet_ad_package_20260610T012525Z \
  use_sim_pose:=true \
  pose_mode:=fixed_pose \
  launch_planning_control:=true \
  rviz:=true
```

## 4. 观测命令

定位：

```bash
ros2 topic echo /localization/pose
```

路网状态：

```bash
ros2 topic echo /planning/roadnet_status
```

仿真 marker：

```bash
ros2 topic echo /simulation/roadnet_markers
```

服务：

```bash
ros2 service list
```

空起点规划：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: '', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

规划结果：

```bash
ros2 topic echo /planning/global_route
ros2 topic echo /planning/trajectory
```

控制输出：

```bash
ros2 topic echo /yunle_chassis/control/scu_control_command
```

## 5. RViz 图层说明

| 图层 | Topic | 说明 |
|---|---|---|
| Roadnet Markers | `/simulation/roadnet_markers` | 基础路网、节点、边、waypoints、语义区域、语义点 |
| Route Markers | `/simulation/route_markers` | 规划全局路线、起点、终点、轨迹线 |
| Planned Trajectory Path | `/simulation/trajectory_path` | nav_msgs/Path 格式轨迹 |
| Vehicle Markers | `/simulation/vehicle_markers` | 当前模拟或真实定位车辆箭头 |

## 6. 安全边界

- 仿真定位只用于调试，不代表真实定位质量。
- 如果真实底盘驱动正在运行，模拟定位和规划轨迹可能导致控制节点输出真实 SCU 命令。
- 实车环境必须保证底盘禁用、轮离地、台架或安全封闭区域，并有 E-stop。
- 当前默认速度保持低速，仍需人工确认 `/yunle_chassis/control/scu_control_command` 的 speed、steering、shift、brake 字段。
