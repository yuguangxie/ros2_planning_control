# Phase 02 — Implement AD Package v1.1 Loader

请实现 `low_speed_av_planning` 中的 RoadnetLoader 和数据模型，严格对齐当前路网编辑器 AD Package ZIP。

必须读取：

```text
project_manifest.json
roadnet/topology.json
trajectory/waypoints.yaml
trajectory/waypoint_index.json
semantics/areas.json
semantics/task_points.json
semantics/parking_points.json
semantics/charging_points.json
validation/validation_report.json
checksums.sha256
```

禁止把旧路径作为主路径：

```text
manifest.json
trajectory/waypoints.json
validation_report.json
```

实现要求：

1. 从 `project_manifest.json` 读取 `schema/schema_version/files/validation/coordinate_system/units`。
2. `schema` 必须是 `low_speed_roadnet_ad_package`。
3. 支持 `schema_version=1.1.0` 和 `1.1.x`。
4. validation failed 或 blocking_errors > 0 时拒绝加载。
5. 优先使用 `manifest.files` 定位文件。
6. 支持 `waypoint_index` 的 `end_index_exclusive` 和旧 `end_index` inclusive。
7. 解析 `waypoints.yaml` 字段 `x/y/yaw/kappa/v_mps/s_m/edge_id/path_id`。
8. 生成内部字段 `x_m/y_m/yaw_rad/kappa_1pm/target_speed_mps/edge_s_m`。
9. 读取 semantics；缺失 optional 文件时 warning。
10. 校验 checksums；当前 sample 必须能通过。

同时生成一个无 ROS2 依赖的脚本：

```text
scripts/validate_sample_ad_package.py
```

它应能验证 `templates/sample_ad_package` 或 `src/low_speed_av_bringup/sample_ad_package`。

创建：

```text
reports/phase_02_report.md
```
