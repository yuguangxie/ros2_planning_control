# Phase 08 Report
- Goal: 生成控制节点、安全停车、限幅和平滑。
- Files changed: `src/low_speed_av_control/include/low_speed_av_control/command_limiter.hpp`, `src/low_speed_av_control/include/low_speed_av_control/command_smoother.hpp`, `src/low_speed_av_control/src/command_limiter.cpp`, `src/low_speed_av_control/src/command_smoother.cpp`, `src/low_speed_av_control/src/control_node.cpp`, `reports/phase_08_report.md`.
- Key design decisions: NaN/Inf、空轨迹、定位超时、轨迹超时、安全急停都进入 controlled stop。
- AD Package compatibility notes: 控制不直接读取 AD Package，只消费 planning 轨迹。
- Config/topic compatibility notes: localization、trajectory、vehicle_state、safety、control_command、control_status 均可配置。
- Tests or offline checks run: 后续离线 smoke 覆盖有限输出。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: 未运行 `/control/command` 发布检查。
- Known limitations: 实时 watchdog 需要 ROS2 runtime 补全验证。
- Next phase handoff: 生成 bringup、配置和文档。
