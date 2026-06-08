# 13 生成工作空间使用说明

本次生成的 ROS2 工作空间保持规划与控制分离：

- `src/low_speed_av_interfaces`：只放 msg/srv 接口。
- `src/low_speed_av_planning`：读取 AD Package、构建拓扑图、全局规划、轨迹拼接和速度规划。
- `src/low_speed_av_control`：车辆模型、控制器、限幅、平滑和安全输出。
- `src/low_speed_av_bringup`：启动文件、默认配置和样例 AD Package。

AD Package 只使用 v1.1 规范路径，例如 `project_manifest.json`、`trajectory/waypoints.yaml`、`validation/validation_report.json`。旧路径 `manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json` 不作为输入。

本环境没有 ROS2。离线验证命令：

```bash
python scripts/validate_expected_tree.py
python scripts/validate_sample_ad_package.py
python scripts/offline_algorithm_smoke.py
```

到 ROS2 环境后再运行：

```bash
source /opt/ros/<distro>/setup.bash
colcon build
colcon test
colcon test-result --verbose
```
