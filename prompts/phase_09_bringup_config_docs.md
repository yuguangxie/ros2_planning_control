# Phase 09 — Bringup, Configs and Runtime Docs

请生成 `low_speed_av_bringup`。

必须包含：

```text
launch/planning_control_demo.launch.py
config/planning_params.yaml
config/control_params.yaml
config/vehicle_params.yaml
sample_ad_package/
docs/runtime_usage.md
```

要求：

1. `planning_control_demo.launch.py` 同时启动 planning 和 control。
2. 参数包含 `roadnet_package_path`。
3. 默认定位 topic 是 `/localization/pose`，可通过 launch argument 或 YAML 修改。
4. sample_ad_package 与当前 AD Package v1.1 对齐：必须有 `project_manifest.json` 和 `trajectory/waypoints.yaml`。
5. 文档说明如何在真实 ROS2 环境运行。
6. 文档说明当前 Codex 环境不运行 colcon。

创建：

```text
reports/phase_09_report.md
```
