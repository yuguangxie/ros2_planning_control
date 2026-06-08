# low_speed_av_bringup

## 模块定位
`low_speed_av_bringup` 提供低速自动驾驶规划/控制系统的启动文件、默认配置和样例 Low Speed Roadnet AD Package v1.1。

该包不包含算法逻辑。它用于把以下模块组合起来：

- `low_speed_av_planning`
- `low_speed_av_control`
- `low_speed_av_interfaces`

## 文件结构

```text
low_speed_av_bringup/
  CMakeLists.txt
  package.xml
  launch/
    planning_control_demo.launch.py
  config/
    planning_params.yaml
    control_params.yaml
    vehicle_params.yaml
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
