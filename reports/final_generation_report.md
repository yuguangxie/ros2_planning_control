# Final Generation Report
- Goal: 生成 Low Speed Roadnet AD Package v1.1 的 ROS2 planning/control workspace。
- Files changed: 新增四个包 `low_speed_av_interfaces`、`low_speed_av_planning`、`low_speed_av_control`、`low_speed_av_bringup`，新增 `scripts/` 离线检查、`docs/13_generated_workspace_usage.md` 和 phase reports。
- Key design decisions: planning/control 分离；算法使用显式 factory；ROS2 不可用时只运行 Python 离线检查。
- AD Package compatibility notes: loader 读取 `project_manifest.json`，支持 `schema=low_speed_roadnet_ad_package` 和 `1.1.x`；使用 `manifest.files`，fallback 到 canonical paths；支持 `end_index_exclusive` 和 legacy inclusive `end_index`；样例包不使用旧路径。
- Config/topic compatibility notes: 默认定位 `/localization/pose`，轨迹 `/planning/trajectory`，全局路线 `/planning/global_route`，控制命令 `/control/command`；所有默认话题都在 YAML 中可配置。
- Tests or offline checks run: `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/validate_expected_tree.py` -> OK；`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/validate_sample_ad_package.py` -> OK，3 nodes、2 edges、6 waypoints；`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/offline_algorithm_smoke.py` -> OK，route `E_L001_F,E_L002_F`，6 trajectory points，Pure Pursuit/Stanley finite commands。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: `source /opt/ros/<distro>/setup.bash`; SKIPPED_ROS2_UNAVAILABLE: `colcon build`; SKIPPED_ROS2_UNAVAILABLE: `colcon test`; SKIPPED_ROS2_UNAVAILABLE: `colcon test-result --verbose`; SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`。
- Known limitations: 当前机器没有 ROS2，未验证 CMake/IDL/节点链接；LQR、frenet_lite、hybrid_astar_parking 保持后续扩展骨架。
- Next phase handoff: 在 ROS2 环境运行 build/test/test-result，并补充节点发布订阅集成测试。

## 审计后两阶段优化状态

### Phase 13：端到端安全语义

- 状态：`PARTIALLY_COMPLETED`
- 当前提交：`44485cc007e6fd4d82d3e90edb0222a0fbb32867`
- `CDX-P0-001`：FIXED。Control 已消费 trajectory emergency/status 并在 controller 前 fail closed。
- `CDX-P0-002`：OPEN。Chassis Driver 仍缺少独立周期 command scheduler、command timeout、startup/shutdown stop 和 watchdog diagnostics。
- ROS2/C++ 执行：`SKIPPED_ROS2_UNAVAILABLE`；Phase 13 的 Python offline PASS 不等价于 ROS2/C++ 回归通过。
- 证据：`reports/phase_13_report.md`。

### Phase 14：自动化测试底座与 CI

- 状态：`IMPLEMENTED_WITH_ACCEPTED_PHASE_13_GAP`
- Gate override：依据 2026-07-11 明确授权继续实施，不再标记为 `BLOCKED_BY_PHASE_13`；该例外不接受或关闭 Chassis watchdog 缺口。
- 已完成：Planning/Control 注册 production-linked gtest；Chassis 建立 `chassis_driver_core` 并为当前 DBC、codec、0x121 frame path 注册 production-linked gtest；Bringup 注册 Planning -> Control -> SCU failure/brake launch test；Python 默认入口、空 parking fixture、统一 runner、template/config/sample 漂移检查和 ROS2 Humble CI 已配置。
- 实际执行：`scripts/run_offline_checks.py` 为 17 PASS / 0 FAIL / 0 SKIPPED；sample、正式 `_1`、正式 `_2` validator 均 PASS；仓库 UTF-8/JSON/Markdown link/hardware-isolation 检查 PASS；`git diff --check` PASS。
- C++/ROS2：48 个 C++ test source cases 与 ROS2 launch source 均未执行，状态为 `GENERATED_NOT_EXECUTED` / `SKIPPED_ROS2_UNAVAILABLE`，不得推断为 PASS。
- CI 状态：`CONFIGURED_NOT_EXECUTED`。workflow 已创建，但没有当前工作区提交的 GitHub Actions 执行证据。
- `CDX-P1-006`：`PARTIALLY_FIXED`；`CDX-P1-007`：`CONFIGURED_NOT_EXECUTED`；`CDX-P3-001/002/003`：`FIXED`。
- `CDX-P0-002`：`OPEN / ACCEPTED_PHASE_14_GAP`。startup/timeout/invalid replay/shutdown/diagnostics watchdog specs 明确跳过，不计为 PASS。
- 证据：`reports/phase_14_report.md`。

### Phase 15：Planning AD Package 与图搜索加固

- 状态：`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI`。
- 基线提交/分支：`b1af553b790c22130699258c86920f9586778bad` / `codex/phase-13-control-safety`；日期 2026-07-11；Windows、无 ROS2/colcon/C++ compiler。
- 已实现：统一 manifest/checksum containment；Loader 结构/数值 fail-closed；admissible A* 与确定性 tie-break；production semantic/current-pose/terminal/progress helper；GlobalRoute/full/local 几何一致性；Planning-only launch source；container Git hygiene 最小修复。
- 测试源码：Planning 三个 production-linked gtest target、40 cases；Planning-only launch 5 个 runtime + 1 个 post-shutdown case。
- 实际执行：FreeCAD Python 3.11.14 运行统一 offline runner，17 PASS / 0 FAIL / 0 SKIPPED；`git diff --check` PASS。C++ 为 `GENERATED_NOT_EXECUTED`，ROS2 为 `SKIPPED_ROS2_UNAVAILABLE`。
- CI：历史 run `29152378189` 是 `EXECUTED_FAIL`（sanitizer PASS、build-test 被 Git ownership 阻塞）；Phase 15 workflow 修复为 `CONFIGURED_NOT_EXECUTED`，没有同提交 PASS 证据。
- Findings：`CDX-P1-005`、`CDX-P2-003/004/005/006` 均为 `PARTIALLY_FIXED / GENERATED_NOT_EXECUTED`；`CDX-P1-007` 未关闭。
- Chassis：`src/yunle_chassis` 零 diff；`CDX-P0-002` 保持 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`，依赖底盘 500 ms 无 0x121 的硬件停车合同。
- 证据：`reports/phase_15_report.md` 与 `docs/PHASE15_TEST_MATRIX.md`。

### Phase 16：Control 工程化与硬件 Watchdog 协同

- 状态：`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI_AND_HIL`。
- 生产实现：启动参数 fail-fast；76/76 YAML leaf 映射；steady-clock 实际 dt smoother；accel/decel/jerk 与独立前后轮 rate；safety bypass；progress/identity；switch/reset；cadence max/p95/missed/deadline diagnostics。
- 默认周期合同：50 Hz/20 ms，软件告警 deadline 100 ms，项目方声明硬件 timeout 500 ms。Control 存活时持续主动发布 stop，不等待硬件 timeout。
- 测试源码：Control 4 个 production-linked gtest target、28 cases；新增 Control-only launch 6 runtime + 1 post-shutdown case。
- 实际执行：统一 offline runner 18 PASS / 0 FAIL / 0 SKIPPED；config 76/76 PASS；repository hygiene、template、Python syntax、`git diff --check` PASS。C++/ROS2 未执行。
- CI：当前工作区 `CONFIGURED_NOT_EXECUTED`；历史 run 仍为 `EXECUTED_FAIL`，没有同提交 full PASS。
- HIL：`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`；未发现供应商协议、CAN 抓包或故障注入证据。
- Chassis：零 diff；`CDX-P0-002` 保持 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`。
- 证据：`reports/phase_16_report.md`、`docs/PHASE16_TEST_MATRIX.md`、`docs/HARDWARE_WATCHDOG_500MS_VALIDATION.md`。

### Phase 17：Ubuntu/Humble 确定性阻断修复

- 状态：`LOCAL_FULL_VALIDATION_PASS / SAME_SHA_CI_PENDING_PUSH_AUTHORIZATION`。
- 基线：`codex/dev`，`6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9`。
- 已修复：标准 `python3-pytest` rosdep metadata；symlink-install integration fixture 与 bundled demo；Humble `assertExitCodes`；Simulation `nav_msgs::msg::Path` reset。
- 本地证据：offline 18/18；7/7 full build；full test/result RC=0；Planning 40/40、Control 28/28、Chassis 7 active + 4 known-gap skip；Bringup 15 active + 1 watchdog skip；Simulation reset 2/2；ASan/UBSan 75 active + 4 known-gap skip。
- Containment：Planning Loader 生产代码零修改；absolute、`..`、mixed separator 和 malicious symlink escape tests 继续 PASS；用户 AD Package override 不受 trusted bundled-default 解析影响。
- rosdep：仅标准 `20-default.list` 时 update/install/check 全 PASS，不依赖 `/etc/ros/rosdep` local mapping。
- CI：`CONFIGURED_NOT_EXECUTED`；未获明确 push 授权，`CDX-P1-007` 仍开放。
- HIL：`HIL_NOT_EXECUTED`；`CDX-P0-002` 保持 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`。
- 证据：`reports/phase_17_report.md` 与 `reports/ubuntu_humble_full_validation_report.md` 第 21 节。

### Phase 18：Control closed-loop kinematic SIL

- 状态：`LOCAL_IMPLEMENTATION_AND_FULL_VALIDATION_PASS / SAME_SHA_CI_NOT_EXECUTED`。
- 新增 ROS-independent `low_speed_av_simulation_core`，中点积分前/双 Ackermann，有限 accel/decel/jerk/steering rate 与 fail-closed timeout/invalid/reverse。
- 新增完整 Planning→Control→`/control/command`→plant→pose/VehicleState launch；Control 为 internal-only，不启动 Chassis/keyboard/UDP/CAN。
- 本地证据：Simulation C++ 25/25；四 controller×两车型 SIL、goal stop、Planning failure stop、node/process isolation PASS；7/7 full build；aggregate 147 tests、0 failure/error、1 known-gap launch skip；ASan/UBSan 100 active production C++ PASS、4 Chassis known-gap SKIP。
- SIL metrics：Control max 0.0206 s、localization max 0.0501 s、lateral RMS/max 0.2745/0.4056 m、goal position/yaw 0.2855 m/0.0322 rad、stopped speed 0.0250 m/s、non-finite 0。
- CI：workflow 已加入 headless SIL、metrics/log artifact 与 Simulation sanitizer，但工作区未 commit/push，状态 `CONFIGURED_NOT_EXECUTED`；baseline SHA 的 run 仍失败。
- HIL：`HIL_NOT_EXECUTED`；`CDX-P0-002` 保持 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`。
- 证据：`reports/phase_18_report.md` 与 `docs/PHASE18_TEST_MATRIX.md`。
