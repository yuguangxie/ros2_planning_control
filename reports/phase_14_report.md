# Phase 14 Report

## Phase status

`IMPLEMENTED_WITH_ACCEPTED_PHASE_13_GAP`

## Gate override

本阶段依据 2026-07-11 的明确授权，在 `CDX-P0-002` 仍为 OPEN 时继续实施，不再标记为 `BLOCKED_BY_PHASE_13`。该例外只解除 Phase 14 门禁，不接受、关闭或弱化 Chassis 独立 watchdog 缺口；Phase 13 仍为 `PARTIALLY_COMPLETED`。

## Goal

为 Planning、Control 和当前 Yunle Chassis 生产实现建立直接链接 production target 的 C++ gtest，准备 Planning -> Control -> SCU ROS2 integration test，修复 Python 默认入口与空 parking fixture，建立 template/config/sample 漂移检查和 ROS2 Humble CI。

## Baseline

- Baseline commit：`44485cc007e6fd4d82d3e90edb0222a0fbb32867`
- Branch：`codex/phase-13-control-safety`
- Date：2026-07-11
- OS：Windows；PowerShell
- ROS distro：unavailable
- `ros2` / `colcon` / `cmake`：unavailable
- C++ compiler：`cl` / `g++` / `clang++` 均 unavailable
- `pytest`：unavailable
- Python：FreeCAD Python 3.11.14
- `uv`：0.10.7
- 修改前非生成型 Python 默认入口：11 PASS、2 FAIL；失败项是 runtime/simulation smoke 指向不存在的 `roadnet_ad_package_20260610T012525Z`
- 修改前 validator：sample `3/2/6`、正式 `_1` `20/26/737`、正式 `_2` `16/22/496` 均 PASS
- 修改前 target：Planning/Control 有 production library；Planning 只有 Python pytest wrapper，Control 有 1 个/6 cases gtest；Chassis 只有 executable，无 core library/gtest；无 GitHub workflow
- Phase 13 核验：`CDX-P0-001` 生产路径仍为 FIXED；`CDX-P0-002` 为 OPEN

## Files changed

### Production target、CMake 与 package metadata

- `src/low_speed_av_planning/CMakeLists.txt`
- `src/low_speed_av_planning/package.xml`
- `src/low_speed_av_control/CMakeLists.txt`
- `src/low_speed_av_bringup/CMakeLists.txt`
- `src/low_speed_av_bringup/package.xml`
- `src/yunle_chassis/chassis_driver/CMakeLists.txt`
- `src/yunle_chassis/chassis_driver/package.xml`
- `src/yunle_chassis/chassis_driver/include/chassis_driver/dbc_protocol.hpp`
- `src/yunle_chassis/chassis_driver/include/chassis_driver/scu_control_frame_builder.hpp`
- `src/yunle_chassis/chassis_driver/src/scu_control_frame_builder.cpp`
- `src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp`

### Test、offline 与 CI

- `src/low_speed_av_planning/test/test_roadnet_loader.cpp`
- `src/low_speed_av_planning/test/test_planning_algorithms.cpp`
- `src/low_speed_av_control/test/test_controllers.cpp`
- `src/low_speed_av_control/test/test_vehicle_command_pipeline.cpp`
- `src/yunle_chassis/chassis_driver/test/test_chassis_core.cpp`
- `src/low_speed_av_bringup/test/test_planning_control_safety_launch.py`
- `scripts/offline_algorithm_smoke.py`
- `scripts/offline_runtime_followup_smoke.py`
- `scripts/offline_simulation_smoke.py`
- `scripts/offline_entrypoint_regression_smoke.py`
- `scripts/check_template_consistency.py`
- `scripts/offline_repository_hygiene.py`
- `scripts/run_offline_checks.py`
- `scripts/validate_expected_tree.py`
- `.github/workflows/ros2_humble_ci.yml`
- `templates/expected_tree.txt`
- `templates/offline_validation/*`
- `templates/sample_config/control_params.yaml`
- `templates/sample_config/planning_params.yaml`

### Documentation and reports

- `README.md`
- `src/low_speed_av_interfaces/README.md`
- `src/low_speed_av_planning/README.md`
- `src/low_speed_av_control/README.md`
- `src/low_speed_av_bringup/README.md`
- `src/yunle_chassis/README.md`
- `docs/08_testing_without_ros2.md`
- `docs/ROS2_INTEGRATION_TEST_PLAN.md`
- `docs/PHASE14_TEST_MATRIX.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `reports/phase_14_report.md`
- `reports/final_generation_report.md`

## Key design decisions

1. C++ tests link the same production library/core used by executables; tests do not copy algorithms, DBC or bit mapping.
2. Chassis extraction is behavior-preserving: ROS callback converts the message, calls pure production `build_scu_control_frame()`, then performs the existing one-shot send. No scheduler/watchdog is added or implied.
3. Chassis tests use an in-memory fake sink and pure codec/frame code. Neither tests nor launch test open UDP sockets or start keyboard control.
4. Watchdog requirements remain executable specifications using `GTEST_SKIP()`/`unittest.skip()` and status `SKIPPED_KNOWN_PRODUCTION_GAP`.
5. `scripts/` is the canonical offline-validation source；production YAML is canonical config；`templates/sample_ad_package` is canonical sample。差异检查比较解析后的 YAML 和 sample SHA-256，不使用不合理的全文件字节相等规则。
6. Python checks are data-contract/consistency evidence only，不作为 production C++ 或 ROS2 行为证明。

## Yunle Chassis refactor summary

`chassis_driver_core` 现在包含 `DbcProtocol`、`CanEthernetCodec` 和 `ScuControlFrameBuilder`，`chassis_driver_node` 与 `test_chassis_core` 链接同一 target。现有 SCU callback 的 shift、steering、speed、brake、flags、CAN ID 0x121、越界置零和非法 shift 丢弃语义保持不变。`DbcProtocol` raw bit helper 成为 production core 的显式测试接口。

本轮没有新增命令缓存、steady-clock command age、周期 scheduler、startup/timeout/shutdown stop 或 diagnostics。

## Production target and test target mapping

| Module | Production target | Test target | Source cases | Positive/negative scope | Safety critical | Current execution |
|---|---|---|---:|---|---:|---|
| Planning loader | `low_speed_av_planning` library | `test_roadnet_loader` | 10 | canonical/1.1.x/two index forms；bad schema/validation/hash/index | yes | GENERATED_NOT_EXECUTED |
| Planning graph/motion/speed | `low_speed_av_planning` library | `test_planning_algorithms` | 9 | Dijkstra/A*/stitch/crop/speed；blocked/reverse/obstacle stub | yes | GENERATED_NOT_EXECUTED |
| Control controllers | `low_speed_av_control` library | `test_controllers` | 4 | four controllers finite/deterministic；empty/single/reverse | yes | GENERATED_NOT_EXECUTED |
| Control vehicle/command | `low_speed_av_control` library | `test_vehicle_command_pipeline` | 8 | two vehicle models/limit/smooth/SCU mapping；NaN/overrange/gear/stop | yes | GENERATED_NOT_EXECUTED |
| Control safety | `low_speed_av_control` library | `test_safety_state_machine` | 6 | 4x2 emergency matrix、timeouts、vehicle gates、latch/clear | yes | GENERATED_NOT_EXECUTED |
| Chassis current production core | `chassis_driver_core` | `test_chassis_core` | 7 active + 4 skip specs | codec/DBC/0x121/fake sink；DLC/trailing/invalid/NaN | yes | GENERATED_NOT_EXECUTED；4 known-gap specs not PASS |
| Planning -> Control -> SCU | production nodes | `test_planning_control_safety_launch.py` | 1 active + 1 known-gap skip + 1 exit check | invalid route -> emergency trajectory -> two brake outputs | yes | SKIPPED_ROS2_UNAVAILABLE |
| Existing Planning Python wrapper | Python data-contract script | `offline_trajectory_continuity` | 1 | route/trajectory fixture continuity | auxiliary | SKIPPED_ROS2_UNAVAILABLE as ament test；runner counterpart PASS |

共有 48 个 C++ `TEST(...)` source cases、4 个 registered Python/launch test methods、8 个 ament/CTest test target 注册。当前实际执行 C++/ROS2 cases 为 0；17 个统一 offline checks 实际执行并通过。4 个 Chassis watchdog C++ specs 和 1 个 launch watchdog spec 明确保留为 `SKIPPED_KNOWN_PRODUCTION_GAP`，不计入 PASS。

## AD Package compatibility notes

未修改 canonical AD Package 数据或合同。仍使用 `project_manifest.json`、`trajectory/waypoints.yaml` 和 `validation/validation_report.json`；sample、正式 `_1`、正式 `_2` validator 均实际通过。Loader 坏数据测试只在 C++ 临时目录中复制 sample 后修改，不污染正式 fixture。

## Config/topic compatibility notes

未修改 Planning、Control、Chassis topic/service 名称、CAN ID、DBC mapping、正式网关默认地址或车辆协议。Control production target 增加完整 export 规则；Chassis core 增加 install/export 规则。template YAML 与 production YAML 的解析结果同步，允许差异表当前为空。

## Tests or offline checks run

实际执行：

```text
C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/run_offline_checks.py
SUMMARY total=17 pass=17 fail=0 skipped=0
```

其中包括：

- expected tree：PASS
- sample validator：PASS，3 nodes / 2 edges / 6 waypoints
- formal `_1` validator：PASS，20 / 26 / 737
- formal `_2` validator：PASS，16 / 22 / 496
- runtime/simulation 默认入口回归：PASS
- 临时空 `parking_points`：PASS，返回 `SKIPPED_EMPTY_PARKING_POINTS`，无 traceback
- template/config/sample consistency：PASS
- UTF-8/JSON/Markdown local link/repository hygiene：PASS，467 text / 74 JSON / 213 Markdown
- `clang-format 14.0.6 --dry-run --Werror`（CI 列出的新增 C++ 文件）：PASS
- 其余现有 offline smoke：PASS
- `git diff --check`：PASS；仅输出一个既有 README CRLF->LF 提示，无 whitespace error

状态统计：

| Category | Source/registered | Actually executed | PASS | FAIL | Other |
|---|---:|---:|---:|---:|---|
| C++ gtest cases | 48 | 0 | 0 | 0 | 48 GENERATED_NOT_EXECUTED；其中 4 为 future known-gap skip specs |
| ROS2/ament Python methods | 4 | 0 | 0 | 0 | SKIPPED_ROS2_UNAVAILABLE；其中 1 为 known-gap skip spec |
| Unified offline checks | 17 | 17 | 17 | 0 | 0 skipped |
| GitHub Actions jobs | 2 configured | 0 | 0 | 0 | CONFIGURED_NOT_EXECUTED |
| HIL | not automated | 0 | 0 | 0 | NOT_EXECUTED |

## ROS2 commands skipped because ROS2 is unavailable

- `colcon build --symlink-install` — `SKIPPED_ROS2_UNAVAILABLE`
- `colcon test --event-handlers console_direct+` — `SKIPPED_ROS2_UNAVAILABLE`
- `colcon test-result --verbose` — `SKIPPED_ROS2_UNAVAILABLE`
- Planning/Control/Chassis production-linked gtest — `GENERATED_NOT_EXECUTED`
- Bringup launch test — `SKIPPED_ROS2_UNAVAILABLE`

未声称上述命令成功。

## CI status

`CONFIGURED_NOT_EXECUTED`

`.github/workflows/ros2_humble_ci.yml` 配置 Ubuntu 22.04 + ROS2 Humble、rosdep、全量 colcon build/test/result、offline runner、clang-format、ASan/UBSan 和 artifact 上传。CI 不启动 Chassis Driver 或 keyboard 节点，不访问真实网关。当前未从 GitHub 获得该 workflow 对本工作区提交的执行证据，因此不能写为 PASS。

## Finding status

| Finding | Status | Evidence |
|---|---|---|
| `CDX-P0-001` | FIXED | `control_node.cpp:341-349,494-499` 保存 emergency/status 并在 controller 前执行 safety decision；既有 `test_safety_state_machine.cpp` 保留 4x2 matrix（GENERATED_NOT_EXECUTED）。 |
| `CDX-P0-002` | OPEN / ACCEPTED_PHASE_14_GAP | `control_command_bridge.cpp:36-47` 仍为 callback 内构帧后单次发送；没有独立 scheduler/watchdog。4 个 gtest specs 与 1 个 launch spec 均不计 PASS。 |
| `CDX-P1-006` | PARTIALLY_FIXED | Planning/Control/Chassis 已有 6 个 production-linked gtest targets、48 cases source；本环境尚未编译执行，且完整需求矩阵仍需 ROS2 执行后扩展。 |
| `CDX-P1-007` | CONFIGURED_NOT_EXECUTED | 当前提交有 CI 配置但没有同 commit 的 ROS2/CI 成功证据；本地只有 Python offline evidence。 |
| `CDX-P3-001` | FIXED | `offline_runtime_followup_smoke.py:104`、`offline_simulation_smoke.py:123` 默认选择现存 `_2`，入口回归实际 PASS。 |
| `CDX-P3-002` | FIXED | `offline_algorithm_smoke.py:132` 对空列表返回可解释 skip；临时 fixture 回归实际 PASS。 |
| `CDX-P3-003` | FIXED | `check_template_consistency.py` 与 runner 实际 PASS，覆盖 validator/config/sample/tree 漂移。 |

## Known limitations

1. 无 ROS2/C++ 工具链，CMake、IDL、gtest 和 launch test 尚未由本机编译执行；源码存在不等于通过。
2. 当前 launch test 只覆盖最关键 failure trajectory -> Control -> SCU brake 链路；成功 route、四 controller runtime switch、timeout、clear service、VehicleState 和 QoS 用例仍需后续补齐并实际执行。
3. Planning/Control gtest 已建立底座但没有覆盖 Prompt 中每一个语义/helper/非法参数组合，因此 `CDX-P1-006` 只标记为 PARTIALLY_FIXED。
4. CI 仅配置未执行；clang-format、ASan/UBSan 和 ROS2 Humble 结果未知。
5. Python replica/smoke 不证明 production C++、DDS、CAN 或 HIL 安全。

## Remaining Phase 13 safety gap

`CDX-P0-002` 仍意味着 Control/DDS/publisher 消失后，Driver 可能继续保持底盘最近接收的运动状态，软件侧没有周期 stop frame 保证。进程硬崩溃或断电还必须依赖底盘硬件 watchdog。本报告和测试底座不构成实车放行依据。

## Next phase handoff

先在独立安全修复中实现并审查 Chassis 最新命令缓存、steady-clock age、20–50 Hz scheduler、startup/invalid/timeout/shutdown stop、同步、diagnostic_msgs 状态和硬件 watchdog 合同；随后启用本阶段保留的 watchdog specs，并在 ROS2 Humble CI/HIL 上保存同 commit 证据。不要将该工作混入第三阶段 loader hardening、A* 修正、闭环仿真或高级算法。
