# Phase 16 Report

## Phase status

`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI_AND_HIL`

## Goal

在不修改 `src/yunle_chassis` 的前提下，完成 Control 参数 fail-fast、真实 dt limiter/smoother、controller fail-closed、tracking progress/reset、周期诊断和 ROS2 回归源码，并建立与项目方声明“500 ms 无 CAN 0x121 硬件停车”的协同边界。

## Baseline

- 日期/环境：2026-07-12，Windows PowerShell，Asia/Shanghai。
- SHA/branch/tracking：`b1af553b790c22130699258c86920f9586778bad`，`codex/phase-13-control-safety`，`origin/codex/phase-13-control-safety`。
- ROS2、colcon、cmake、cl、g++、clang++、pytest：不可用。
- 可用：uv；FreeCAD Python 3.11.14 + PyYAML 6.0.3。
- 修改前 offline runner：17 PASS / 0 FAIL / 0 SKIPPED。
- 修改前 Control：3 个 production-linked gtest target、18 source cases；Phase 14 sanitizer 对旧 18 cases 实际 PASS。
- 工作区含 Phase 15 未提交改动和用户已有 `docs/prom_cdx/`、`reports/post_phase_14_recommendations.md`，均保留。
- 历史 CI run `29152378189`：sanitizer PASS，主 build-test FAIL；当前基线总体 `EXECUTED_FAIL`。
- 硬件证据：只发现项目方 500 ms 声明；没有供应商协议、CAN 抓包、bench/HIL 或固件矩阵。

## Files changed

- Control production：`command_smoother`、四 controller、两 vehicle model、Safety trajectory validation、ControlNode；新增 `control_runtime_helpers`。
- Control tests/CMake：扩展三个既有 gtest，新增 `test_control_runtime_helpers`。
- Config/governance：Control、Bringup、template config；新增 `check_control_config_contract.py` 并加入 runner。
- Integration/CI：新增 Control-only launch test，Bringup 注册；workflow 增加定向 Control/Bringup test/result。
- 文档：Control README/design/config/SCU/LQR/operator、Phase 16 test matrix、500 ms HIL 规程、audit 和 reports。
- 未修改：`src/yunle_chassis/**`、Planning production、Interfaces 字段、正式 Roadnet、CAN/DBC/topic/service/网关配置。

## Key design decisions

1. `validate_control_configuration` 是 ROS-independent production validator。Node 读取全部参数后统一验证，非法安全参数抛异常，节点启动失败。
2. 删除 silent no-op YAML：`localization_pose_type`、`hold_last_command_s`、未使用车身外形、旧 fixed speed step、统一 steer rate、emergency decel、stop mode，以及被 curvature list 遮蔽的 MPC sample-count/max-curvature。
3. Smoother 接收实际 steady-clock dt，分别限制加速、减速、jerk、前轮 rate 和后轮 rate；异常 dt clamp 并诊断。Safety stop 先于普通平滑并固定为零速/零转角/brake/disable。
4. Tracking progress 在 Node/controller 前使用 trajectory identity、单调 index、有限窗口、heading 和 gear。新 trajectory、算法/模型切换、estop clear 和重新使能 reset。
5. 当前 reverse 没有专用控制模型，四 controller 统一 fail closed 为 `unsupported_reverse_tracking`。
6. Cadence 使用 fake-clock 可测 production helper，记录 last/max/p95 interval、publish age、missed cycles 和 deadline misses。检测到 >=500 ms 的 Control gap 后，恢复周期先发布 stop 并经过 READY interlock；这不是 Chassis 软件 watchdog。

## Parameter and cadence contract

- 配置映射：76 YAML leaves / 76 declared / 76 consumed，实际 Python governance PASS。
- 默认 Control rate：50 Hz，nominal period 20 ms。
- 软件 deadline warning：100 ms；不作为正常 jitter 预算。
- 项目方硬件失联声明：500 ms；是 20 ms nominal 的 25 倍、100 ms warning 的 5 倍。生产参数校验禁止把该诊断上界配置为大于 500 ms。
- Hardware status：`DECLARED_NOT_HIL_VERIFIED`。
- Control 存活且上游异常：持续周期发布 active stop，不等待 500 ms。
- Control/DDS/Driver/主机/网络使 0x121 完全消失：由外部硬件合同接管；软件不声明制动力和停车距离。

## Production target and test target mapping

| Production target | Test target | Source cases | Local execution |
|---|---|---:|---|
| `low_speed_av_control` | `test_controllers` | 6 | GENERATED_NOT_EXECUTED |
| `low_speed_av_control` | `test_vehicle_command_pipeline` | 11 | GENERATED_NOT_EXECUTED |
| `low_speed_av_control` | `test_safety_state_machine` | 6 | GENERATED_NOT_EXECUTED |
| `low_speed_av_control` | `test_control_runtime_helpers` | 5 | GENERATED_NOT_EXECUTED |
| Control node | `test_control_runtime_launch.py` | 6 runtime + 1 post-shutdown | SKIPPED_ROS2_UNAVAILABLE |
| Planning + Control nodes | `test_planning_control_safety_launch.py` | existing failure/brake case | SKIPPED_ROS2_UNAVAILABLE |

四个 gtest target 都在 `if(BUILD_TESTING)` 注册并链接同一个 production library。当前 source 总数 28，实际 C++ 执行 0。

## AD Package compatibility notes

未修改 Planning production 或 canonical AD Package 合同，未修改 sample/正式 `_1`/`_2`。Integration 只消费现有 Trajectory 接口。

## Config/topic compatibility notes

`output.mode=both`、三个输出 topic、clear/set-controller service、SCU 字段和单位保持。删除的仅是未实现/no-op 配置 key。Reverse 由原“透传 gear 但使用前进模型”收紧为显式 stop，是保守安全行为变化。

## Tests or offline checks run

- 修改前 runner：17/17 PASS。
- 修改后 runner：18 PASS / 0 FAIL / 0 SKIPPED。
- `check_control_config_contract.py`：PASS，76/76/76。
- Template consistency：PASS，Phase 16 required tree complete。
- sample、正式 `_1`、正式 `_2` validator：PASS。
- Python syntax、workflow YAML、repository UTF-8/JSON/Markdown link/hardware isolation：PASS。
- `git diff --check`：PASS（最终复核）。
- Chassis/正式 Roadnet diff：空（最终复核）。

## ROS2 commands skipped because ROS2 is unavailable

- 定向 Control/Bringup build/test/result：`SKIPPED_ROS2_UNAVAILABLE`。
- 全量 build/test/result：`SKIPPED_ROS2_UNAVAILABLE`。
- Control C++ 4 targets：`GENERATED_NOT_EXECUTED`。
- Bringup launch tests：`SKIPPED_ROS2_UNAVAILABLE`。
- clang-format：通过 `uvx clang-format 22.1.8` 对 Phase 15/16 workflow 门禁文件实际格式化并执行 `--dry-run --Werror`，结果 PASS；Ubuntu CI 仍需用仓库安装版本复验。

## CI status

`CONFIGURED_NOT_EXECUTED`（当前工作区）。Workflow 配置 full build/test/result、定向 Control/Bringup、sanitizer 和 artifacts，但本任务未提交/推送，不能写 PASS。历史 run `29152378189` 仍为 `EXECUTED_FAIL`。

## HIL status

`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`

证据来源只有用户/项目方声明。待执行 wheels-off/bench 的 Control exit、Driver exit、network loss、host power loss 和 CAN timestamp 矩阵见 `docs/HARDWARE_WATCHDOG_500MS_VALIDATION.md`。

## Finding status

| Finding | Status |
|---|---|
| CDX-P0-001 | FIXED_PRODUCTION / REGRESSION_SKIPPED_ROS2_UNAVAILABLE |
| CDX-P0-002 | OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION |
| CDX-P1-004 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED |
| CDX-P1-006 | PARTIALLY_FIXED |
| CDX-P1-007 | EXECUTED_FAIL / CURRENT_WORKTREE_CONFIGURED_NOT_EXECUTED |
| CDX-P2-006 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED |
| CDX-P2-007 | OPEN_CAPABILITY / FAIL_CLOSED_MITIGATION |
| CDX-P2-010 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED |
| CDX-P2-011 | FIXED_PRODUCTION / REGRESSION_GENERATED_NOT_EXECUTED |

## Known limitations

1. Phase 16 C++、CMake、ROS2 launch/QoS 尚未由当前源码实际编译执行。
2. 100 ms cadence launch 断言是待执行的 CI 合同，不是本机观测结果。
3. Reverse tracking 能力仍未实现；当前行为是安全停车。
4. ModuleStatus 字段有限，诊断采用结构化文本；尚未引入 `diagnostic_msgs`。
5. 500 ms 外部合同没有供应商/HIL 证据，不能据此计算停车距离或放行实车。

## Next phase handoff

下一步只应在 Ubuntu ROS2 Humble 构建并运行当前定向/全量 C++ 与 launch tests，修复编译和 flaky/QoS 问题，取得同提交 CI PASS；随后在安全台架执行 500 ms CAN 故障注入并归档证据。不要自动进入闭环仿真、reverse 高级控制或完整 MPC。
