# Phase 17 Report

## Phase status

`LOCAL_FULL_VALIDATION_PASS / SAME_SHA_CI_PENDING_PUSH_AUTHORIZATION`

## Goal

只修复 Ubuntu 22.04 / ROS2 Humble 实测确认的四个确定性阻断：标准 rosdep metadata、`--symlink-install` 合法 sample、Humble launch post-shutdown API，以及 Simulation reset 编译/回归。不进入闭环仿真、reverse controller、Chassis 软件 watchdog 或 HIL。

## Baseline

- 分支：`codex/dev`
- 基线 SHA：`6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9`
- 开始工作区：仅有未跟踪的 `reports/ubuntu_humble_full_validation_report.md`，已保留并继续更新。
- 环境：Ubuntu 22.04.5 LTS、ROS2 Humble、`/usr/bin/python3` 3.10.12、GCC 11.4.0、CMake 3.22.1。
- Phase 17 验证根：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_phase17_20260712_230907`

四个基线失败均在修改前重新保存：

- `baseline/rosdep_standard_metadata_failure.log`
- `baseline/launch_fixture_and_humble_api_failures.log`
- `baseline/launch_test_result.log`
- `baseline/simulation_compile_failure.log`

## Files changed

- Bringup metadata/CI：`src/low_speed_av_bringup/package.xml`、`.github/workflows/ros2_humble_ci.yml`。
- Bringup launch/integration：`planning_control_demo.launch.py` 和三份既有 launch tests。
- Simulation production/test：`sim_localization_pose_publisher_node.cpp`、Simulation CMake/package metadata、新增 `test_sim_localization_reset_launch.py`。
- Offline governance：runtime/template expected-tree 以及 Phase 17 required-tree 检查。
- Reports：本报告、Ubuntu Humble 历史验证报告、final generation report。
- 未修改：Planning Loader/搜索算法、Control 算法、`src/yunle_chassis/**`、正式 Roadnet、CAN/DBC/SCU contract。

## Four root causes and fixes

| Root cause | Fix | Evidence |
|---|---|---|
| Bringup 使用 Jammy 标准 rosdep 不认识的 `pytest` key | 改为标准 `python3-pytest`；CI build-test/sanitizer 均增加 `rosdep check` | 仅保留标准 `20-default.list` 时 update/install/check 全 RC=0 |
| symlink install 将 installed sample 文件链接回源码，Loader canonical containment 正确拒绝 | 两份 Planning integration 各自将 installed sample 物化复制到独立临时根；Planning 实际参数指向物化根；测试结束清理 | ready、PlanRoute、task/parking/charging mission、invalid goal/reload 全 PASS |
| demo 默认 bundled sample 同样经过逐文件 symlink | demo 只对仓库控制的默认 bundled manifest 解析可信物理根；用户 override 不解析、不改写，仍交给 Loader strict containment | 实际 `ros2 launch` smoke 观测 `loaded AD package pkg_demo_ad_package_v1_1` |
| Humble `ProcInfoHandler` 不提供 `assertWaitForShutdown` | 三份 post-shutdown tests 使用 Humble 的 `launch_testing.asserts.assertExitCodes`，默认仅接受退出码 0；CMake TIMEOUT 保持 bounded | 三份 post-shutdown methods 全 PASS |
| `nav_msgs::msg::Path` 没有 `clear()` | reset 时以全新 `nav_msgs::msg::Path{}` 替换历史，再追加当前 initial pose | Simulation build PASS；reset service/path/header/process-exit 2 methods PASS |

物化 charging fixture 修改内容后，同时重算 charging 文件 hash、manifest 内 hash，以及 `checksums.sha256` 中 charging 与 `project_manifest.json` 两项，避免测试 fixture 绕过或关闭 checksum 验证。

## Containment safety

`roadnet_loader.cpp` 零修改。absolute、Windows/UNC absolute、`..`、mixed separator、checksum escape 和 canonical symlink escape 的生产逻辑均保持原样。Planning Loader 19/19 tests 继续 PASS，其中 malicious internal symlink escape 在 Linux 实际执行且拒绝；没有 `GTEST_SKIP()`。

demo 的物理根解析只用于未提供 `roadnet_package_path` override 时的仓库 bundled sample。任意用户 AD Package 参数不做可信化处理，仍必须让目标文件 canonical 后全部位于用户 package root 内。

## Rosdep clean metadata result

主机上一阶段的 Phase 17 local `pytest` mapping 已删除。为排除预装第三方 rosdep source 的影响，复验时临时只保留标准 `20-default.list`：

| Command | RC |
|---|---:|
| `rosdep update` | 0 |
| `rosdep install --from-paths src --ignore-src -r -y` | 0 |
| `rosdep check --from-paths src --ignore-src -r` | 0 |

复验后已恢复主机原有 `40-autonomoustuff-public-humble.list`；仓库和 Phase 17 不依赖它或任何 local mapping。

## Local build and test results

| Layer | Registered/source | PASS | FAIL | SKIPPED | Status |
|---|---:|---:|---:|---:|---|
| Offline | 18 | 18 | 0 | 0 | PASS |
| Planning production C++ | 40 | 40 | 0 | 0 | PASS |
| Control production C++ | 28 | 28 | 0 | 0 | PASS |
| Chassis production C++ | 11 | 7 | 0 | 4 | PASS_WITH_KNOWN_GAP |
| Bringup launch methods | 16 | 15 | 0 | 1 | PASS_WITH_KNOWN_GAP |
| Simulation reset methods | 2 | 2 | 0 | 0 | PASS |
| Full packages | 7 | 7 | 0 | 0 | PASS |
| Sanitizer production C++ | 79 | 75 | 0 | 4 | PASS_WITH_KNOWN_GAP |

- Directed build：7/7 packages，RC=0。
- Directed `colcon test-result`：RC=0；聚合显示 111 tests、0 errors、0 failures、1 skipped。该聚合包含 CTest wrappers；源码层 skip 实际为 4 个 Chassis specs + 1 个 Bringup watchdog method。
- Full build：7/7 packages，RC=0。
- Full test/result：RC=0；同样为 111 aggregated、0 errors、0 failures、1 reported skipped。
- Simulation：编译完成；reset service 成功；移动历史在 reset 后回到一个 initial pose；path/pose frame 为 `phase17_map`；新 header 时间晚于旧 path；进程退出码 0。
- ASan/UBSan：build/test/result 全 RC=0；75 active production-linked C++ PASS、4 known-gap SKIP，无 sanitizer 错误。
- workflow 的 21 个 clang-format 文件：PASS；Python compileall、workflow YAML、`git diff --check`：PASS。

## Launch method result

Bringup 15 个 active methods 全 PASS：Planning canonical ready、PlanRoute、task/parking/charging PlanMission、invalid goal failure stop、invalid reload fail closed、Planning-to-Control/SCU brake、Control cadence/controller/VehicleState/timeout/estop/QoS，以及三个 Humble-compatible bounded exit methods。`test_chassis_publisher_loss_triggers_watchdog_stop` 保持 `SKIPPED_KNOWN_PRODUCTION_GAP`。

invalid goal test 不再错误要求 trajectory `status` 等于字面值 `failure`；production 该字段保存具体失败原因。新断言同时要求 `emergency_stop`、`trajectory_id=failure_stop`、非空原因、全部零速点和 `planning_failure_stop` behavior，安全条件更具体而非放宽。

## GitHub Actions

- 当前 SHA：仍为基线 `6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9`；Phase 17 尚未 commit。
- Phase 17 workflow：`CONFIGURED_NOT_EXECUTED`。
- GitHub Actions URL：`PENDING_PUSH_AUTHORIZATION`。
- `CDX-P1-007` 不能关闭；必须在获得明确授权后创建并推送独立 Phase 17 commit，再等待该 SHA 的 `build-test`、`sanitizers` 和 artifact。

## Finding status

| Finding | Phase 17 status | Rationale |
|---|---|---|
| `CDX-P1-005` | `FIXED_LOCAL_VALIDATED / CI_PENDING` | 原 containment 保持 FIXED；normal、bundled symlink install、absolute/`..`/mixed 和 malicious symlink matrix 本机 PASS |
| `CDX-P1-007` | `OPEN / LOCAL_FULL_PASS / SAME_SHA_CI_NOT_EXECUTED` | 本机 full PASS，但尚无 Phase 17 commit 的 CI |
| `CDX-P2-005` | `FIXED_PRODUCTION_TESTED` | helper C++ 与 task/parking/charging mission/route integration 实际 PASS |
| `CDX-P2-006` | `FIXED_PRODUCTION_UNIT / SERVICE_RUNTIME_PASS / EXPLICIT_LOOP_PROGRESS_RUNTIME_NOT_ADDED` | Planning/Control progress C++ PASS，Planning service runtime 已恢复；本阶段不扩张为新的 loop runtime 项 |
| `CDX-P0-002` | `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION` | Chassis 软件 watchdog 不在范围，5 个 specs 继续 SKIP |
| HIL | `HIL_NOT_EXECUTED` | 没有真实硬件访问或故障注入 |

## Hardware isolation

测试与 CI 未引用真实 `192.168.*`、keyboard control 或 `chassis_driver_node`。本轮只启动 Planning、Control 和 Simulation；没有启动真实 Chassis Driver，没有打开 UDP/CAN socket，没有发送真实运动命令。

## Remaining risks

1. Phase 17 尚未形成新 commit，也没有同 SHA GitHub Actions 和 artifact 证据。
2. `CDX-P0-002` Chassis 软件 watchdog 仍开放；500 ms 仍是 `DECLARED_NOT_HIL_VERIFIED`。
3. Reverse controller、闭环 plant 和 HIL 明确不在 Phase 17 范围。

## Phase 18 handoff

在明确获得 push 授权后，只完成 Phase 17 commit/push、等待同 SHA 两个 CI jobs 与 artifact，并回填最终 URL/SHA。若 CI 全 PASS，Phase 17 才可标记完成；之后停止，不自动进入 Phase 18。
