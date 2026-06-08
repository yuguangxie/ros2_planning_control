# Phase 11 — Final Generation Report

请生成最终报告：

```text
reports/final_generation_report.md
```

报告必须包含：

1. 生成的 package 列表。
2. AD Package v1.1 对齐说明。
3. 当前读取的 canonical 文件路径。
4. 规划模块算法列表和完成度。
5. 控制模块算法列表和完成度。
6. 阿克曼模型支持情况：front_ackermann 和 dual_ackermann。
7. `/localization/pose` 可配置说明。
8. 无 ROS2 环境下已经运行的离线检查。
9. 因 ROS2 不可用而跳过的命令。
10. 真实 ROS2 环境后续命令：

```bash
source /opt/ros/$ROS_DISTRO/setup.bash
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
```

11. P0/P1/P2 剩余 TODO。
12. 如何接入真实路网编辑器导出的 ZIP。
13. 如何切换规划和控制算法。
14. 如何切换前转和前后双转阿克曼。

不要声称 `colcon build` 已经通过，除非你确实在 ROS2 环境运行了。
