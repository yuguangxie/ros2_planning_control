# Phase 15 测试矩阵

基线提交：`b1af553b790c22130699258c86920f9586778bad`；分支：`codex/phase-13-control-safety`；日期：2026-07-11；本地环境：Windows、无 ROS2/colcon/C++ compiler，FreeCAD Python 3.11.14。

| 层级 | Production target | Test target/source | 主要范围 | 本地状态 |
|---|---|---|---|---|
| C++ unit | `low_speed_av_planning` | `test_roadnet_loader` | canonical、schema/validation/hash/index、path containment、重复 ID、数值、no-go | GENERATED_NOT_EXECUTED |
| C++ unit | `low_speed_av_planning` | `test_planning_algorithms` | Dijkstra/A*、blocked/reverse/unreachable、tie-break、admissible cost、motion/speed | GENERATED_NOT_EXECUTED |
| C++ unit | `low_speed_av_planning` | `test_planning_helpers` | current/semantic anchor、terminal stop、summary、continuity、speed zone、loop progress/reset | GENERATED_NOT_EXECUTED |
| ROS2 launch | Planning node | `test_planning_services_launch.py` | canonical ready、PlanRoute、task/parking/charging mission、failure estop、invalid reload、QoS/republish/exit | SKIPPED_ROS2_UNAVAILABLE |
| Python data contract | scripts/sample/formal fixtures | `run_offline_checks.py` | sample、正式 `_1`/`_2`、template、JSON/UTF-8/link/hardware isolation | PASS 17/17 |

C++ source 共 40 个 `TEST` case，三个 target 均在 `if(BUILD_TESTING)` 注册并链接同一 production library；本机实际执行 0。Planning-only launch source 含 5 个运行期用例和 1 个 post-shutdown 用例；本机实际执行 0。

ROS2 Humble 复现：先 `colcon build --symlink-install --packages-up-to low_speed_av_planning low_speed_av_bringup`，再 `colcon test --packages-select low_speed_av_planning low_speed_av_bringup --event-handlers console_direct+` 与 `colcon test-result --verbose`。Python 复现：使用带 PyYAML 的 Python 执行 `python scripts/run_offline_checks.py`。
