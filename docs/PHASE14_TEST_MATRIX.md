# Phase 14 测试矩阵

测试基线提交：`44485cc007e6fd4d82d3e90edb0222a0fbb32867`；分支：`codex/phase-13-control-safety`；日期：2026-07-11；本地环境：Windows、无 ROS2/colcon/cmake/C++ compiler。

| 模块 | Production target | Test target | 类型 | 主要正例 | 主要负例 | 当前状态 |
|---|---|---|---|---|---|---|
| Planning loader | `low_speed_av_planning` | `test_roadnet_loader` | C++ gtest | canonical sample、1.1.x、两种 end index | schema、validation、blocking、checksum、坏 index | GENERATED_NOT_EXECUTED |
| Planning algorithms | `low_speed_av_planning` | `test_planning_algorithms` | C++ gtest | Dijkstra/A*、stitch、speed | unreachable、blocked、reverse disabled、obstacle stub | GENERATED_NOT_EXECUTED |
| Control controllers | `low_speed_av_control` | `test_controllers` | C++ gtest | 4 controller 有限/确定输出 | empty trajectory fail closed | GENERATED_NOT_EXECUTED |
| Control pipeline | `low_speed_av_control` | `test_vehicle_command_pipeline` | C++ gtest | 2 vehicle models、limit/smooth、SCU units/gears | NaN、overrange、unknown/PARK、安全 stop | GENERATED_NOT_EXECUTED |
| Control safety | `low_speed_av_control` | `test_safety_state_machine` | C++ gtest | READY/ACTIVE、clear、4×2 matrix | emergency/failure/timeout/fault/brake/latch | GENERATED_NOT_EXECUTED |
| Chassis current core | `chassis_driver_core` | `test_chassis_core` | C++ gtest | codec、DBC、0x121、fake sink | trailing、DLC、invalid shift、NaN/overrange | GENERATED_NOT_EXECUTED |
| Chassis watchdog | 不存在 | `ChassisWatchdogKnownGap.*` | C++ gtest specification | 无 | startup/timeout/cache/shutdown/diagnostics | SKIPPED_KNOWN_PRODUCTION_GAP |
| Planning→Control→SCU | production nodes | bringup launch test | ROS2 integration | failure trajectory 传播到双 brake output | invalid goal | SKIPPED_ROS2_UNAVAILABLE |
| AD/data contracts | Python scripts | `run_offline_checks.py` | Python | sample、正式 `_1/_2`、连续性、UTF-8/JSON/Markdown link | 临时空 parking/bad fixture、真实设备地址 | 17/17 PASS |
| Template/config | templates + production config | `check_template_consistency.py` | Python | canonical mirror/hash | script/config/sample drift | PASS |
| HIL | 真实底盘 | 未自动化 | HIL | 低速跟踪 | 崩溃、断链、断电、硬件 watchdog | NOT_EXECUTED |

## 单项复现

```bash
colcon test --packages-select low_speed_av_planning --ctest-args -R test_roadnet_loader
colcon test --packages-select low_speed_av_control --ctest-args -R test_vehicle_command_pipeline
colcon test --packages-select chassis_driver --ctest-args -R test_chassis_core
colcon test --packages-select low_speed_av_bringup --ctest-args -R planning_control_safety
python3 scripts/check_template_consistency.py
python3 scripts/offline_entrypoint_regression_smoke.py
```

未执行的 C++/ROS2 项不能从本矩阵推断为 PASS。
