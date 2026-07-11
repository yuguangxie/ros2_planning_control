# Phase 13 Report

## Goal

修复 Planning 到 Control 的安全语义丢失、VehicleState 门控、锁存急停显式清除和默认控制输出合同，并为这些逻辑建立可离线验证的状态机与测试。用户在实施过程中追加约束：本轮不得修改 `src/yunle_chassis`，已产生的 Chassis Driver 改动必须恢复。因此本阶段的 Control 范围已完成，Chassis 独立命令 watchdog 未实施，阶段总状态为 **PARTIALLY_COMPLETED**。

## Files changed

- Control 生产代码：
  - `src/low_speed_av_control/include/low_speed_av_control/control_node.hpp`
  - `src/low_speed_av_control/include/low_speed_av_control/control_types.hpp`
  - `src/low_speed_av_control/include/low_speed_av_control/safety_state_machine.hpp`
  - `src/low_speed_av_control/src/control_node.cpp`
  - `src/low_speed_av_control/src/safety_state_machine.cpp`
  - `src/low_speed_av_control/src/command_smoother.cpp`
- 依赖与测试：
  - `src/low_speed_av_control/CMakeLists.txt`
  - `src/low_speed_av_control/package.xml`
  - `src/low_speed_av_control/test/test_safety_state_machine.cpp`
  - `scripts/offline_phase13_safety_smoke.py`
  - `scripts/offline_remaining_fixes_smoke.py`
  - `scripts/validate_expected_tree.py`
- 配置：
  - `src/low_speed_av_control/config/control_params.yaml`
  - `src/low_speed_av_bringup/config/control_params.yaml`
- 文档：
  - `README.md`
  - `src/low_speed_av_control/README.md`
  - `docs/03_ros2_interfaces.md`
  - `docs/05_control_module_design.md`
  - `docs/07_config_launch_runtime.md`
  - `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
  - `docs/OPERATOR_STARTUP_CHECKLIST.md`
  - `docs/CONTROL_MODULE_CODE_WALKTHROUGH.md`
  - `docs/PLANNING_CONTROL_INPUT_GUIDE.md`
  - `docs/ROS2_COMMAND_EXAMPLES.md`

`src/yunle_chassis` 的临时修改已全部恢复；该目录相对 Git 基线无工作区 diff、无暂存 diff。

## Key design decisions

1. 增加 ROS-independent `ControlSafetyStateMachine`，明确 `WAIT_INPUTS`、`READY`、`ACTIVE`、`CONTROLLED_STOP`、`ESTOP_LATCHED`；只有 `ACTIVE` 能调用控制器。
2. `Trajectory` 入口保存并验证 `trajectory_id`、`source_package_id`、`status`、`emergency_stop`、接收时间及点内容。默认仅接受 `status=ok`；空 ID、空点、NaN/Inf、非法 gear、明显非单调 `s_m` 均 fail closed。
3. failure/emergency trajectory 在 controller 和 vehicle model 之前被截断，因此 Pure Pursuit、Stanley、LQR、MPC sampler 与 front/dual Ackermann 共用相同停车语义。
4. VehicleState 保存并门控 `autonomous_enabled`、`brake_pressed`、`fault_code`；支持 `vehicle_state.required`。一旦收到，即使 `required=false`，失效、超时、自治关闭、人工制动或故障也禁止运动。
5. localization、trajectory、VehicleState watchdog 使用 `std::chrono::steady_clock` 的 receive time，不依赖零 header stamp、ROS time 或暂停的仿真时钟。
6. `SafetyEstopLatch` 保证普通 OK/standby 不清除锁存急停。新增 `/low_speed_av_control/clear_estop`（`std_srvs/srv/Trigger`）；清除必须满足输入新鲜、存在有效 VehicleState、车辆静止、无故障、未踩制动且自治许可，成功后先进入 `READY`。
7. 所有安全停车统一为零速、零前后轮转角、`brake=1`、`enable=false` 和非空 reason；smoother 对安全停车立即旁路。SCU 映射为 target speed 0 与 brake enable true。
8. 默认 `output.mode=both`，同时发布 `/control/command`、`/control/status` 与 `/yunle_chassis/control/scu_control_command`。
9. 新增安全参数采用 fail-fast；rate/timeout 必须有限且大于 0，速度/转角/减速度限制必须有限且非负，stop shift 与 output mode 必须属于允许集合。

## AD Package compatibility notes

- 未修改 canonical AD Package 合同、正式 Roadnet 数据或 Planning loader。
- 仍使用 `project_manifest.json`、`trajectory/waypoints.yaml`、`validation/validation_report.json`。
- sample、正式包 `_1`、正式包 `_2` 均通过现有校验；未引入旧 `manifest.json`、`trajectory/waypoints.json` 或根目录 `validation_report.json`。

## Config/topic compatibility notes

- 两份 Control 配置同步为 `output.mode: both`，并新增：
  - `controller.allowed_trajectory_statuses: ["ok"]`
  - `controller.trajectory_s_tolerance_m: 1.0e-4`
  - `vehicle_state.required: false`
  - `vehicle_state.timeout_s: 0.5`
  - `safety.clear_speed_threshold_mps: 0.05`
- 默认 topic 保持：`/localization/pose`、`/planning/trajectory`、`/vehicle/state`、`/safety/status`、`/control/command`、`/control/status`、`/yunle_chassis/control/scu_control_command`。
- 现有 launch 已从上述参数文件加载配置，不需要新增 launch 参数。
- 新增标准依赖 `std_srvs` 和测试依赖 `ament_cmake_gtest`；未新增或修改自定义 msg/srv 字段。

## Tests or offline checks run

环境检查：`ros2`、`colcon`、`cl`、`g++`、`clang++`、`cmake` 均不可用；`uv` 和 FreeCAD Python 3.11.14 可用。

修改前基线：13 项既有 Python validator/smoke 在显式传入正确 package 路径后全部通过。`offline_runtime_followup_smoke.py` 与 `offline_simulation_smoke.py` 需要显式传入 `roadnet_ad_package_20260610T012525Z_2` 才能验证正式包；未伪造默认路径 PASS。

修改后执行并通过：

- `validate_expected_tree.py`
- `validate_sample_ad_package.py src/low_speed_av_bringup/sample_ad_package`
- `validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_1`
- `validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z_2`
- `offline_algorithm_smoke.py`
- `offline_remaining_fixes_smoke.py`
- `offline_reverse_policy_smoke.py`
- `offline_runtime_followup_smoke.py roadnet_ad_package_20260610T012525Z_2`
- `offline_scu_lqr_smoke.py`
- `offline_semantic_goal_followup_smoke.py`
- `offline_sim_localization_follow_smoke.py`
- `offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z_2`
- `offline_trajectory_continuity_smoke.py`
- `offline_phase13_safety_smoke.py`
- Python `py_compile`：通过。
- `git diff --check`：通过。

新增 Python 行为测试覆盖 4 controllers × 2 vehicle models、emergency/failure/non-finite trajectory、定位/轨迹/VehicleState 超时、自治关闭、人工制动、故障、普通 OK 不清 latch、clear 拒绝/成功、READY interlock 和默认双输出。新增 C++ gtest 还覆盖真实 `CommandSmoother` 安全旁路和 `ScuCommandMapper` 制动映射，但因无 C++/ROS2 环境未执行。

## ROS2 commands skipped because ROS2 is unavailable

- `colcon build --symlink-install --packages-up-to low_speed_av_control chassis_driver low_speed_av_bringup` — `SKIPPED_ROS2_UNAVAILABLE`
- `colcon test --packages-select low_speed_av_control chassis_driver` — `SKIPPED_ROS2_UNAVAILABLE`
- `colcon test-result --verbose` — `SKIPPED_ROS2_UNAVAILABLE`
- 全量 `colcon build/test` — `SKIPPED_ROS2_UNAVAILABLE`
- `ros2 service call /low_speed_av_control/clear_estop std_srvs/srv/Trigger "{}"` — `SKIPPED_ROS2_UNAVAILABLE`

没有声称 ROS2/C++ 已构建成功。

## Finding status

| Finding | Status | Evidence |
|---|---|---|
| CDX-P0-001 | **FIXED** | `control_node.cpp:334-351` 消费轨迹元数据；`safety_state_machine.cpp:22-93,96-151` 在 controller 前拒绝 emergency/failure/非法轨迹；`test_safety_state_machine.cpp:32,59` 与 `offline_phase13_safety_smoke.py` 验证 4×2 一致停车。 |
| CDX-P0-002 | **OPEN** | 用户最新指令禁止本轮修改 Yunle Chassis，所有临时改动已恢复。现状仍在 `control_command_bridge.cpp:39-98` 的 subscription callback 单次发送，`chassis_driver_node.cpp:132` 析构仅停线程；无独立 scheduler/watchdog/diagnostics。 |
| CDX-P1-001 | **FIXED** | `control_node.cpp:354-370` 保存 VehicleState 字段与 receive time；`safety_state_machine.cpp:32-48` 实施 fault/autonomous/brake/timeout 门控；C++ test `TimeoutsAndVehicleGatesStopTracking` 和 Phase 13 Python smoke 通过。 |
| CDX-P1-002 | **FIXED** | `control_node.cpp:373-381` 只更新 latch，不由 OK 清除；`control_node.cpp:434-456` 仅 Trigger 显式清除；`safety_state_machine.cpp:8-20,166-207` 提供可测试 latch/clear 条件；相关 C++/Python 测试通过。 |
| CDX-P1-003 | **FIXED** | 构造默认 `output.mode=both`（`control_node.cpp:64`），两份配置分别为 `control_params.yaml:10` 与 bringup `control_params.yaml:6`；`control_node.cpp:584-603` 默认发布 internal 与 SCU，status publisher 始终启用。 |

## Known limitations

1. **CDX-P0-002 仍开放**：Chassis Driver 没有独立周期调度、startup/timeout/shutdown stop 或 diagnostics；Control 持续发布 stop 不能替代 Driver watchdog。
2. Driver 进程硬崩溃、断电、UDP 网关或 CAN 链路故障必须依赖底盘硬件 watchdog；软件层无法覆盖。
3. C++ gtest、ROS2 service 行为和端到端 topic/CAN 行为尚未在 ROS2 Humble 环境编译执行。
4. 当前 SCU 只有布尔制动字段，controlled stop 与 hard estop 都映射为 brake enable；不同制动力曲线需底盘协议/执行器侧支持。
5. simulation 只能验证消息与状态机；bench 必须架车/断开驱动轮并具备物理急停；vehicle 必须完成硬件 watchdog 和失联停车验证。

## Next phase handoff

在用户允许修改 `src/yunle_chassis` 后，单独关闭 CDX-P0-002：将 0x121 callback 改为仅验证和缓存，加入 20–50 Hz scheduler、steady-clock command timeout、startup/invalid/timeout/shutdown stop、线程同步与 `diagnostic_msgs/DiagnosticArray`，并增加纯 C++ watchdog/frame 测试。随后在真实 ROS2 Humble 环境执行本报告列出的定向与全量 build/test，以及 sim、bench、vehicle 分级验收。

本报告不启动第二阶段测试体系或第三阶段 loader hardening。
