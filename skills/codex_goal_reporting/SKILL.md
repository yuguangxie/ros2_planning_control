# Skill: Codex Goal Mode Reporting

Use this skill in every phase.

## Required Report File

Each phase must create:

```text
reports/phase_xx_report.md
```

Format:

```markdown
# Phase XX Report

## Goal

## Files changed

## Key design decisions

## AD Package compatibility notes

## Topic and config compatibility notes

## Tests or offline checks run

## ROS2 commands skipped

## Known limitations

## Next phase handoff
```

## Final Report

Create:

```text
reports/final_generation_report.md
```

Include:

- generated package list,
- planning algorithms implemented,
- control algorithms implemented,
- AD Package v1.1 compatibility checklist,
- no-ROS2 validation results,
- commands to run in real ROS2 environment,
- remaining TODOs ranked P0/P1/P2.

## Important

Do not claim ROS2 build succeeded unless it actually ran in a ROS2 environment.
