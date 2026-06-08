# Phase 00 — Discovery and Implementation Plan

请只扫描当前仓库，不要大规模改代码。目标是确认现有目录结构、是否已有 ROS2 包、是否已有 interfaces、是否已有路网 sample，然后制定实施计划。

必须遵守：当前环境没有 ROS2，不要运行 `colcon build` 作为验收。

请输出：

```text
reports/phase_00_report.md
```

报告必须包含：

1. 当前仓库结构摘要。
2. 是否已存在 `src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_bringup`。
3. 当前是否存在 AD Package sample。
4. 当前是否有旧路径假设：`manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json`。
5. 修改计划，明确以 `project_manifest.json`、`trajectory/waypoints.yaml`、`validation/validation_report.json` 为准。
6. 每阶段计划。
7. ROS2 命令跳过说明。
