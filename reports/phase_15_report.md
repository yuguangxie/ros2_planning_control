# Phase 15 Report

## Phase status

`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI`

生产代码、CMake、production-linked tests、Planning-only launch source、最小 CI 修复和文档均已完成；本地无 ROS2/C++ 工具链，不能标记完整完成或 finding FIXED。

## Goal

在 `src/yunle_chassis` 零修改前提下，加固 Planning AD Package 路径/结构安全，修正 Dijkstra/A* 成本与确定性合同，将 semantic/trajectory/progress 逻辑抽到 production helper，并建立 Planning ROS2 回归闭环源码。

## Baseline

- 日期/OS：2026-07-11，Windows PowerShell。
- SHA/branch：`b1af553b790c22130699258c86920f9586778bad`，`codex/phase-13-control-safety`，tracking `origin/codex/phase-13-control-safety`。
- 工具：ROS2、colcon、cmake、cl、g++、clang++、pytest 均不可用；`uv 0.10.7` 可用；FreeCAD Python 3.11.14 + PyYAML 6.0.3 可用。
- 工作区已有未跟踪用户内容：`docs/prom_cdx/`、`reports/post_phase_14_recommendations.md`，已保留。
- 基线 CI：[run 29152378189](https://github.com/yuguangxie/ros2_planning_control/actions/runs/29152378189) 为 `EXECUTED_FAIL`；sanitizer job PASS，full build-test 在 `git ls-files` exit 128 失败。

## Files changed

- Planning production：Loader、TopologyGraph、Dijkstra、A*、PlanningNode；新增 `planning_helpers.hpp/.cpp`。
- Planning tests/CMake：扩展 loader/algorithm tests，新增 `test_planning_helpers` 并注册 production-linked target。
- Bringup integration：新增 `test_planning_services_launch.py` 并注册；fixture 仅在临时目录构造。
- CI/hygiene：repository safe-directory、`git ls-files` stderr/return-code 处理、workflow container ownership。
- 文档：Planning README、AD Package/Planning/semantic/output/continuity/reverse 文档、审计、测试矩阵、Phase 14/final report、本报告。
- 未修改：`src/yunle_chassis/**`、Control production、正式 Roadnet 包、bringup/template sample。

## Key design decisions

1. 统一 `resolve_contained_path`：将反斜杠标准化，拒绝 POSIX/Windows/UNC 绝对路径和 `..`，再以 `weakly_canonical` 检查 symlink 后目标仍在 canonical root 内。Manifest files、hashes、checksums 和最终读取共用该逻辑。
2. Loader 双层 fail closed：除了 schema/validation/hash，增加 ID、引用、有限性、非负、range/count/overlap/coverage/edge 与 semantic 唯一性检查。
3. A* 用全图最小 `edge.cost / endpoint_distance` 缩放直线距离；默认权重可采纳，权重大于 1 明确标为 weighted/non-optimal。Dijkstra/A* 均使用稳定 ID tie-break，并自行拒绝手工图的负/非有限 cost。
4. `planning_helpers` 属于同一 production library；Node 和 gtest 共用 anchor、terminal、summary、continuity、speed 与 progress 实现。
5. Progress tracker 用 trajectory identity、单调 index、有限搜索窗口和 heading；package/reload/cache clear/algorithm switch reset。
6. GlobalRoute summary 按实际 full reference geometry 重算，避免追加 semantic edge/node 后 length/time 仍是旧值。

## AD Package compatibility notes

继续只使用 canonical `project_manifest.json`、`trajectory/waypoints.yaml`、`validation/validation_report.json`，支持 1.1.0 与 1.1.x、exclusive 与 legacy inclusive end index。未引入旧合同，未修改 sample/正式 `_1`/`_2` 数据。路径与结构检查比旧版本严格；恶意或损坏包会明确拒绝。

## Production target and test target mapping

| Production target | Test target | Source cases | Local execution |
|---|---|---:|---|
| `low_speed_av_planning` | `test_roadnet_loader` | 19 | GENERATED_NOT_EXECUTED |
| `low_speed_av_planning` | `test_planning_algorithms` | 13 | GENERATED_NOT_EXECUTED |
| `low_speed_av_planning` | `test_planning_helpers` | 8 | GENERATED_NOT_EXECUTED |
| Planning node | `test_planning_services_launch.py` | 5 runtime + 1 post-shutdown | SKIPPED_ROS2_UNAVAILABLE |

三个 gtest target 均在 `if(BUILD_TESTING)` 内注册并链接同一个 production library，没有复制算法。

## Config/topic compatibility notes

未改变 topic、service、自定义 msg/srv 字段或默认低速配置。新增 progress 参数在 Node 内有保守默认值，未强迫旧 YAML 增加 key。Planning-only integration 不启动 Control/Chassis，不访问 UDP 或真实底盘。

## Tests or offline checks run

- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/run_offline_checks.py`：17 PASS / 0 FAIL / 0 SKIPPED。
- sample、正式 `_1`、正式 `_2` validator：全部 PASS。
- expected tree、algorithm、entrypoint、remaining fixes、reverse、runtime、semantic、simulation、continuity、Phase 13 safety、template consistency、UTF-8/JSON/Markdown link/hardware isolation：全部 PASS。
- Python `py_compile`：PASS。
- `git diff --check`：PASS（最终复核见下方执行记录）。
- Source counts：C++ 40，实际 C++ 执行 0；新增 Planning launch 6，实际 ROS2 执行 0。

第一次以 uv Python 3.14 无 PyYAML 运行 runner 得到依赖性 FAIL；随后按仓库既有环境约定使用含 PyYAML 的 FreeCAD Python 重跑并全 PASS。前一次结果是环境依赖诊断，不作为最终 PASS 证据。

## ROS2 commands skipped because ROS2 is unavailable

- 定向 `colcon build`：`SKIPPED_ROS2_UNAVAILABLE`。
- 定向 `colcon test` / `colcon test-result`：`SKIPPED_ROS2_UNAVAILABLE`。
- 全量 build/test/result：`SKIPPED_ROS2_UNAVAILABLE`。
- 三个 Planning gtest target：`GENERATED_NOT_EXECUTED`。
- 两个 Bringup launch target：`SKIPPED_ROS2_UNAVAILABLE`。

## CI status

- 基线 run `29152378189`：`EXECUTED_FAIL`；sanitizer PASS，full build-test FAIL。
- 当前 Phase 15 工作区：`CONFIGURED_NOT_EXECUTED`。已修复 safe-directory 和错误输出，但本任务未获授权提交/推送，因而没有同提交 CI URL/SHA 或 PASS 证据。

## Finding status

| Finding | Status | Evidence / close condition |
|---|---|---|
| CDX-P1-005 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED | Production containment + absolute/relative/mixed/symlink/checksum tests；需实际 C++ PASS。 |
| CDX-P1-006 | PARTIALLY_FIXED | Planning 3 production-linked targets、40 cases；新增 cases 未执行。 |
| CDX-P1-007 | EXECUTED_FAIL | 没有当前工作区 full CI PASS。 |
| CDX-P2-003 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED | Loader 结构/数值校验已实现；负例未执行。 |
| CDX-P2-004 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED | Admissible/weighted/tie-break/negative contract 已实现；等价与 cost tests 未执行。 |
| CDX-P2-005 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED | Full-reference route summary 与 terminal helper 已实现；C++/launch 未执行。 |
| CDX-P2-006 | PARTIALLY_FIXED / GENERATED_NOT_EXECUTED | Planning progress window 已实现；helper test 未执行。 |
| CDX-P0-002 | OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION | 依赖底盘 500 ms 无 0x121 硬件停车；本阶段 Chassis 零 diff。 |

## Known limitations

1. C++/CMake/ROS IDL/launch 源码尚未在 ROS2 Humble 编译执行，可能仍存在工具链暴露的编译或 QoS 问题。
2. Symlink escape 在不允许创建 symlink 的平台会 `GTEST_SKIP`；Linux CI 必须实际执行该 case。
3. Planning progress 已限制；Control controller 自身的回环最近点行为不在本阶段范围。
4. Weighted A* 权重大于 1 明确不保证最优，生产默认仍为 1。
5. Hardware watchdog 合同不是 Planning 的软件测试结果，也不关闭 `CDX-P0-002`。

## Next phase handoff

Phase 16 先在 Ubuntu ROS2 Humble 对当前提交执行定向和全量 build/test/result，修复编译、launch/QoS/flaky 问题并取得 required CI 同提交 PASS；随后再处理 Control 的 controller 轨迹 progress/loop 与更完整 ROS2 回归。不得把未执行源码提前改写为 FIXED。

## Phase 16 实施注记（2026-07-12）

Phase 16 已在同一未提交工作区完成 Control production/config/test/docs 修改，但本地仍无 ROS2/C++，状态为 `IMPLEMENTED_LOCALLY_PENDING_ROS2_CI_AND_HIL`。Phase 15 Planning 文件未被回退；`src/yunle_chassis` 仍为零 diff。详细结果见 `reports/phase_16_report.md`。
