# Fix Plan 5

## Objective

按优先级给出第 5 轮审计后的修复计划。

## Scope

所有 P0/P1/P2 风险。

## Status

Plan only。

## Evidence

- `docs/audits_5/RISK_REGISTER_5.md` 给出了 P1/P2 风险的优先级来源。
- `docs/audits_5/TESTING_AND_OFFLINE_SMOKE_AUDIT.md` 记录了当前仅完成离线脚本验证，ROS2 build/test/launch 仍为 `SKIPPED_ROS2_UNAVAILABLE`。
- `docs/audits_5/CURRENT_POSE_START_PLANNING_AUDIT.md` 和 `docs/audits_5/SIMULATION_VISUALIZATION_AUDIT.md` 记录了当前位置起点匹配和仿真可视化的主要待验证点。

## Fix Plan

| Phase | Target | Files likely affected | Exact fix description | Acceptance criteria | Suggested Codex prompt |
|---|---|---|---|---|---|
| 1 | ROS2 build blockers | `src/**/CMakeLists.txt`, `package.xml`, source files | 在 Ubuntu/ROS2 执行 colcon build，修复所有编译、链接、接口生成错误。 | `colcon build --symlink-install` 通过。 | “在 ROS2 环境中构建当前 workspace，修复所有编译/链接错误，不改变功能合同。” |
| 2 | Simulation launch validation | `src/low_speed_av_simulation/launch`, `rviz`, CMake | 修复 launch/RViz/package share 问题。 | `ros2 launch low_speed_av_simulation ... rviz:=true` 可启动并显示 markers。 | “验证并修复 low_speed_av_simulation launch/RViz，使 roadnet markers 和 pose marker 可见。” |
| 3 | Current-pose matching correctness | `planning_node.cpp`, matcher helper/tests | 实现 edge projection、方向一致性、route prefix/crop 或明确中段起点策略。 | 中段 pose、反向边、交叉口测试通过。 | “升级 planning current-pose matcher 为 edge projection，并补充离线/ROS2 测试。” |
| 4 | Safety hardening for sim/live separation | simulation launch/config/docs | 增加 namespace 或 explicit bench flag，避免模拟定位误接 live vehicle。 | 默认不会误启动全链路真实底盘输出，文档清晰。 | “为仿真定位增加防误用安全开关和 namespace 推荐，不破坏现有用法。” |
| 5 | SCU bench validation | docs/scripts/control | 增加 bench validation helper 和记录模板，确认 driver 接收。 | chassis driver 接收 SCU topic，stop/estop 正确。 | “添加 Yunle SCU bench 验证脚本和结果记录，不改变 mapper 合同。” |
| 6 | RViz usability | `roadnet_visualization_node.cpp`, rviz | 按 edge 分开 waypoint markers，增加 labels/status。 | 无跨边伪连线，路线/轨迹清晰。 | “改进 RViz roadnet visualization，按 edge 分 Marker，保留现有 topic。” |
| 7 | Test coverage | `scripts/`, C++ tests | 增加 C++/ROS2 launch tests 和 current-pose failure tests。 | CI 或手工可覆盖 build、service、topic。 | “为 planning/control/simulation 增加 ROS2 可运行的 smoke tests。” |
| 8 | Documentation cleanup | `docs/` | 更新旧文档中过时的 planning 未订阅定位描述和 line refs。 | `rg` 不再出现冲突描述。 | “清理旧文档中过时内容，使 current-pose start 行为一致。” |

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-FIX-001 | P1 | Open | 第一步必须是真实 ROS2 build。 | 后续验证都依赖可构建 workspace。 | Phase 1。 | build log。 |

## ROS2 Commands Run Or Skipped

SKIPPED_ROS2_UNAVAILABLE in current environment.

## Remaining Uncertainty

Fix plan 需根据 ROS2 build/launch 实测结果调整。
