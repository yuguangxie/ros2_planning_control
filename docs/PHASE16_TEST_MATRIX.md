# Phase 16 测试矩阵

基线：`b1af553b790c22130699258c86920f9586778bad`，分支 `codex/phase-13-control-safety`，日期 2026-07-12，Windows，无 ROS2/colcon/C++ compiler。

| 层级 | Production target | Test target/source | 范围 | 当前状态 |
|---|---|---|---|---|
| C++ unit | `low_speed_av_control` | `test_controllers` | 四 controller、nominal/empty/single/zero/NaN/reverse/确定性 | GENERATED_NOT_EXECUTED |
| C++ unit | `low_speed_av_control` | `test_vehicle_command_pipeline` | 两车型、limiter、真实 dt accel/decel/jerk、前后 rate、reset/bypass、SCU | GENERATED_NOT_EXECUTED |
| C++ unit | `low_speed_av_control` | `test_safety_state_machine` | 状态优先级、timeout、VehicleState、latch/clear、4×2 emergency | GENERATED_NOT_EXECUTED |
| C++ unit | `low_speed_av_control` | `test_control_runtime_helpers` | 参数 fail-fast、progress/identity、四 controller window、fake-clock cadence | GENERATED_NOT_EXECUTED |
| ROS2 launch | Control node | `test_control_runtime_launch.py` | both/cadence、switch/reset、vehicle gates、三类 timeout、estop clear、late subscriber | SKIPPED_ROS2_UNAVAILABLE |
| ROS2 launch | Planning + Control | `test_planning_control_safety_launch.py` | Planning failure/emergency 到 internal/SCU brake | SKIPPED_ROS2_UNAVAILABLE |
| Python governance | Control config | `check_control_config_contract.py` | production/bringup/template 相等，YAML leaf declare/get 76/76 | PASS |
| Python offline | repository | `run_offline_checks.py` | 数据、模板、配置、UTF-8/JSON/links/isolation | PASS（18/18） |
| Bench/HIL | 真实底盘 | `HARDWARE_WATCHDOG_500MS_VALIDATION.md` | 0x121 消失、Control/Driver/network/host fault | HIL_NOT_EXECUTED |

当前 Control C++ source 共 28 个 `TEST` case，四个 target 均直接链接 production library。本机实际执行 0。Phase 14 sanitizer 的 18 PASS 绑定旧基线，不证明 Phase 16 新实现通过。
