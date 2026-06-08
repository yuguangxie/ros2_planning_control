# 10 Codex Goal Mode Usage

## Master Goal

Send:

```text
prompts/goal_00_master_goal.md
```

Use this when Codex can handle a large generation task in one goal.

## Staged Goals

Use staged prompts when:

- Codex context becomes too large,
- you want review between phases,
- you want reliable phase reports,
- you want to avoid large unreviewed changes.

## What to Tell Codex About ROS2

Always include this instruction:

```text
当前运行环境没有 ROS2。不要运行 colcon build 作为必须验收项。请生成源码和配置，并用纯 Python 离线脚本验证文件结构、AD Package 契约和算法逻辑。所有 ROS2 构建命令写入报告的“真实 ROS2 环境后续命令”部分。
```

## How to Review After Each Phase

Check:

1. `reports/phase_xx_report.md` exists.
2. Files changed match phase scope.
3. AD Package paths still use current schema.
4. `/localization/pose` remains configurable.
5. Planning/control boundaries are not mixed.
6. No accidental Nav2/Lanelet2 dependency was introduced.
7. No fake claim of `colcon build` success appears.
