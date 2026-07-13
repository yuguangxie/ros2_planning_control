# Ubuntu 22.04 / ROS2 Humble Full Validation Report

## 1. 结论

目标提交不能判定为完整通过。

- 目标版本匹配：`codex/dev`，`6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9`。
- 定向 build：PASS，6/6 packages 完成。
- production-linked C++：Planning 40/40 PASS；Control 28/28 PASS；Chassis 7 active PASS，4 watchdog specs 为 `SKIPPED_KNOWN_PRODUCTION_GAP`。
- ROS2 integration：FAIL。16 个源码 test methods 全部执行，8 PASS、7 FAIL（含 3 errors）、1 SKIPPED。
- 全量 build：FAIL。7 packages 中 6 完成、1 失败、0 中止；因此 full test 未执行。
- ASan/UBSan：build/test PASS；75 active C++ cases PASS，4 known-gap specs SKIPPED。
- GitHub Actions：目标 SHA 的 run `29185828794` 为 `EXECUTED_FAIL`；sanitizers job PASS，build-test job FAIL。
- HIL：`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`。
- 后续依赖修复复验：交互式 sudo 下 `apt-get update/install`、`rosdep update/install/check` 均 PASS；重新构建和测试后，上述源码/测试基础设施失败保持不变，确认不是 sudo 或缺失依赖造成。

本次没有修改生产代码、CMake、package metadata、测试、配置、sample、正式 Roadnet、template sample 或 Chassis；没有 commit/push，也没有启动 keyboard control、Chassis Driver、真实 UDP/CAN 或运动命令。

## 2. 版本、环境与隔离目录

| Field | Actual |
|---|---|
| Repository | `/home/xie/planning_control/ros2_planning_control/ros2_planning_control` |
| Branch | `codex/dev` |
| HEAD | `6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9` |
| Commit | `6b07d6c Complete planning and control validation phases` |
| Remote | `origin https://github.com/yuguangxie/ros2_planning_control.git` |
| Initial worktree | clean |
| Target mismatch | no |
| OS | Ubuntu 22.04.5 LTS (Jammy) |
| Kernel | `6.8.0-124-generic` x86_64 |
| ROS | Humble |
| Python used for validation | `/usr/bin/python3`, 3.10.12 |
| CMake | 3.22.1 |
| Compiler | GCC/G++ 11.4.0 |
| colcon | `/usr/bin/colcon` |
| clang-format | Ubuntu 14.0.0 |
| RMW observed in build | `rmw_cyclonedds_cpp` |
| Validation root | `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155` |
| Dependency-remediation revalidation root | `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217` |

登录环境原先激活 `/home/xie/miniconda3`。所有 ROS2、colcon、offline、format 和 sanitizer 命令均重置 PATH、清除 Conda/Python 环境变量，并使用系统 Python。构建、安装、CTest 结果和 colcon 日志均位于仓库外的 validation root。

## 3. 依赖安装与 rosdep

初次非交互执行结果如下；这些结果解释了第一次依赖安装为何未完成，但没有阻止当时使用已有系统依赖开展验证：

| Initial command | RC | Result |
|---|---:|---|
| `sudo -n apt-get update` | 1 | FAIL：`sudo: a password is required` |
| `sudo -n apt-get install ...` | 1 | FAIL：`sudo: a password is required` |
| `rosdep update` | 0 | PASS |
| `sudo -n rosdep install --from-paths src --ignore-src -r -y` | 1 | FAIL：sudo password required |
| `rosdep check --from-paths src --ignore-src -r` | 2 | FAIL：`low_speed_av_bringup` 的 rosdep key `pytest` 在 Ubuntu Jammy 无定义 |

用户授权交互式 sudo 后完成修复，密码仅送入 sudo 的交互式标准输入，未写入命令、文件、日志或仓库：

| Remediation command | RC | Result |
|---|---:|---|
| `sudo apt-get update` | 0 | PASS |
| `sudo apt-get install -y build-essential cmake python3-pip python3-pytest python3-yaml clang-format ros-humble-ament-cmake-gtest ros-humble-launch-testing ros-humble-launch-testing-ament-cmake` | 0 | PASS；所需包均安装，3 个 ROS test packages 升级 |
| `sudo rosdep init` | 0 | PASS；补回缺失的标准 `20-default.list` |
| `rosdep update` | 0 | PASS |
| `rosdep resolve pytest` | 0 | PASS，解析为 `python3-pytest` |
| `sudo rosdep install --from-paths src --ignore-src -r -y` | 0 | PASS |
| `rosdep check --from-paths src --ignore-src -r` | 0 | PASS：`All system dependencies have been satisfied` |

Ubuntu Jammy 的标准 rosdep 数据没有仓库 package.xml 所用的非标准 key `pytest`。为保持仓库不变，在主机 `/etc/ros/rosdep/` 中新增了最小本地映射，将 `pytest` 映射到已安装的 `python3-pytest`，并由 rosdep 实际解析验证。该系统级映射不是仓库改动；其他干净主机若不提供同等映射，仍会复现这个 rosdep metadata 可移植性问题。复验使用升级后的 `ament_cmake_gtest 1.3.14`、`launch_testing 1.0.14`、`launch_testing_ament_cmake 1.0.14`。

## 4. Package 与 target 清单

`colcon list` 共 7 packages：

1. `chassis_interfaces`
2. `chassis_driver`
3. `low_speed_av_interfaces`
4. `low_speed_av_planning`
5. `low_speed_av_control`
6. `low_speed_av_bringup`
7. `low_speed_av_simulation`

Production-linked target 注册和链接核验：

| Module | Test target | Source cases | Production link | Registered |
|---|---|---:|---|---|
| Planning | `test_roadnet_loader` | 19 | `low_speed_av_planning` | yes |
| Planning | `test_planning_algorithms` | 13 | `low_speed_av_planning` | yes |
| Planning | `test_planning_helpers` | 8 | `low_speed_av_planning` | yes |
| Control | `test_controllers` | 6 | `low_speed_av_control` | yes |
| Control | `test_vehicle_command_pipeline` | 11 | `low_speed_av_control` | yes |
| Control | `test_safety_state_machine` | 6 | `low_speed_av_control` | yes |
| Control | `test_control_runtime_helpers` | 5 | `low_speed_av_control` | yes |
| Chassis | `test_chassis_core` | 11 | `chassis_driver_core` | yes |

Bringup 注册 3 个 launch targets：`test_planning_control_safety_launch.py`、`test_planning_services_launch.py`、`test_control_runtime_launch.py`。源码共有 16 个 test methods（含 post-shutdown 和 1 个 known-gap skip）。

## 5. 汇总统计

以下按源码 case/method 统计；不把 CTest wrapper XML 与内部 gtest/xUnit XML 重复相加。Chassis gtest XML 将 4 项标为 `result="skipped"`，虽然 `colcon test-result` 的聚合摘要误报 skipped=0，本报告按实际 gtest 结果修正。

| Category | Registered | Executed | PASS | FAIL | SKIPPED | Status |
|---|---:|---:|---:|---:|---:|---|
| Offline | 18 | 18 | 18 | 0 | 0 | PASS |
| Planning C++ | 40 | 40 | 40 | 0 | 0 | PASS |
| Control C++ | 28 | 28 | 28 | 0 | 0 | PASS |
| Chassis C++ | 11 | 11 | 7 | 0 | 4 | PASS_WITH_KNOWN_GAP |
| ROS2 launch | 16 | 16 | 8 | 7 | 1 | FAIL |
| Full colcon | 7 packages | 7 build attempts | 6 packages built | 1 package build fail | 0 | FAIL_BUILD_TEST_NOT_RUN |
| Sanitizers | 79 C++ cases | 79 | 75 | 0 | 4 | PASS_WITH_KNOWN_GAP |
| GitHub Actions | 2 jobs | 2 | 1 | 1 | 0 | EXECUTED_FAIL |
| HIL | 0 | 0 | 0 | 0 | 0 | HIL_NOT_EXECUTED |

## 6. Offline、格式与仓库治理

`python3 scripts/run_offline_checks.py`：RC=0，`18 PASS / 0 FAIL / 0 SKIPPED`。

| Check | Actual result |
|---|---|
| expected tree | PASS |
| sample package | PASS：3 nodes / 2 edges / 6 waypoints |
| formal package `_1` | PASS：20 / 26 / 737 |
| formal package `_2` | PASS：16 / 22 / 496 |
| algorithm sample | PASS |
| entrypoint regressions | PASS；空 parking 返回可解释 skip，无 traceback |
| remaining fixes | PASS |
| reverse policy | PASS |
| runtime followup | PASS |
| SCU/LQR | PASS |
| semantic goal | PASS |
| simulation/localization offline | PASS |
| trajectory continuity | PASS，正式 `_1`/`_2` 场景均通过 |
| Phase 13 safety contract | PASS，4 controllers × 2 models |
| Control config contract | PASS：76 YAML leaves / 76 declared / 76 consumed |
| template consistency | PASS |
| repository hygiene | PASS：493 UTF-8 text files、74 JSON、230 Markdown；本地 links 检查 PASS |
| compileall | PASS |
| `git diff --check`（报告前） | PASS |

独立重复执行 `check_control_config_contract.py`、`check_template_consistency.py`、`offline_repository_hygiene.py` 均 RC=0。

Bringup sample 与 template sample 的聚合树 hash 相同：

```text
bd26b309bcbd31d6f28901269c116976a9060498ac5bc12ebc1ccdc6e1346f18
```

workflow 原列出的 21 个 C++/header 文件使用系统 clang-format 14 原样执行 `--dry-run --Werror`：RC=0，PASS；没有执行 `-i`。

## 7. 定向构建

命令按要求使用 `--symlink-install --packages-up-to low_speed_av_planning low_speed_av_control low_speed_av_bringup chassis_driver`。

- `DIRECTED_BUILD_RC=0`
- 6 packages finished：`chassis_interfaces`、`low_speed_av_interfaces`、`chassis_driver`、`low_speed_av_planning`、`low_speed_av_control`、`low_speed_av_bringup`
- failed=0，aborted=0
- 构建使用 `/usr/bin/python3`，没有 Conda Python 参与。

日志：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/directed/log/build_2026-07-12_16-43-15/`

## 8. Planning production C++

| Target | Registered | Executed | PASS | FAIL | SKIPPED |
|---|---:|---:|---:|---:|---:|
| `test_roadnet_loader` | 19 | 19 | 19 | 0 | 0 |
| `test_planning_algorithms` | 13 | 13 | 13 | 0 | 0 |
| `test_planning_helpers` | 8 | 8 | 8 | 0 | 0 |

实际 PASS 覆盖：canonical/schema/version/validation/hash/index；绝对、混合分隔符、`..`、checksum 和 Linux symlink containment；重复 ID、非法引用、负与非有限数值；Dijkstra/A* 最优 cost、确定性、blocked/reverse/unreachable、weighted A* 声明；motion/speed；semantic anchor/terminal、route summary/continuity、speed zone、progress/reset。

Planning package 还执行 1 个辅助 pytest `offline_trajectory_continuity` 并 PASS；它未计入上表 40 个 production C++ cases。

## 9. Control production C++

| Target | Registered | Executed | PASS | FAIL | SKIPPED |
|---|---:|---:|---:|---:|---:|
| `test_controllers` | 6 | 6 | 6 | 0 | 0 |
| `test_vehicle_command_pipeline` | 11 | 11 | 11 | 0 | 0 |
| `test_safety_state_machine` | 6 | 6 | 6 | 0 | 0 |
| `test_control_runtime_helpers` | 5 | 5 | 5 | 0 | 0 |

实际 PASS 覆盖：四 controller；两种 Ackermann model；空、零速、NaN/Inf、negative/reverse fail closed；limiter；真实 dt accel/decel/jerk；前后轮独立 rate；reset；安全 bypass；VehicleState 门控；estop latch/clear/READY interlock；4×2 emergency；progress/identity/window；fake-clock cadence；SCU mapper 和 PARK/unknown fixed brake stop。

## 10. Chassis core

`test_chassis_core`：11 executed，7 PASS，0 FAIL，4 SKIPPED。

7 个 active PASS 覆盖 DBC Intel/Motorola bit、signed/unsigned、13-byte codec、DLC/trailing bytes、CAN ID `0x121`、shift/speed/front/rear steering/brake/flags、invalid shift、NaN/Inf/越界归零，以及内存 fake sink。测试源码没有 `UdpChannel`/socket 创建；没有启动 Chassis Driver 或打开真实 UDP。

以下 4 项保持 `SKIPPED_KNOWN_PRODUCTION_GAP`，不得计为 PASS：

- startup stop
- timeout stop
- invalid command does not replay old motion
- shutdown stop and diagnostics

## 11. ROS2 integration

执行环境设置 `LOW_SPEED_AV_CHASSIS_TEST_MODE=1`，没有启动真实 Chassis Driver。

整体：FAIL。源码 method 计数为 16 executed / 8 PASS / 7 FAIL / 1 SKIPPED。`colcon test-result` 因额外统计 3 个 CTest wrapper 显示 19 tests、3 errors、7 failures、1 skipped；本报告不混淆两种计数。

### 11.1 通过项

- Planning failure/invalid goal -> emergency trajectory -> Control internal brake stop + SCU brake stop：PASS。
- Control 默认 `output.mode=both`：PASS。
- internal/SCU 周期输出与小于 150 ms 的测试门限：PASS。
- 四 controller runtime switch/reset：PASS。
- autonomous disabled、brake pressed、fault 门控：PASS。
- localization、trajectory、VehicleState timeout：PASS。
- estop latch、普通 OK 不直接清除、clear 拒绝/成功、成功后 READY：PASS。
- late subscriber 收到后续周期 command：PASS。
- invalid reload fail closed：PASS。

### 11.2 失败项与最后状态

首个 Planning 根因：

```text
planning inactive: manifest resolves outside the AD package root: project_manifest.json
```

`--symlink-install` 将安装空间的 sample 文件链接回源码树；Loader 对 symlink 的 canonical containment 正确地看到真实目标位于安装 package root 外，因此拒绝合法的已安装 sample。由此引发：

- canonical ready / transient-local RoadnetStatus：timeout FAIL；最后状态为 planning inactive，routes=0，trajectories=0。
- PlanRoute success/republish：FAIL；`roadnet package not loaded`。
- task/parking/charging PlanMission：在 task 阶段 FAIL；`roadnet package not loaded`，后续目标未能形成成功证据。
- Planning-only invalid goal emergency assertion：timeout FAIL；最后 trajectories=102，但没有满足 `emergency_stop && status == failure` 的消息。

独立测试基础设施根因：全部 3 个 post-shutdown methods 在 Humble 上 ERROR：

```text
AttributeError: 'ProcInfoHandler' object has no attribute 'assertWaitForShutdown'
```

实际进程日志显示节点已收到 SIGINT 并 clean exit，但测试调用了 Humble `ProcInfoHandler` 不提供的方法，因此 bounded-exit 断言本身未通过。

1 个 skip 是 `test_chassis_publisher_loss_triggers_watchdog_stop`，保持 `SKIPPED_KNOWN_PRODUCTION_GAP`。

日志：

- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/directed/log/test_2026-07-12_16-44-04/`
- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/directed/build/low_speed_av_bringup/test_results/low_speed_av_bringup/`

## 12. 全量 build/test

`FULL_BUILD_RC=2`。

| Metric | Actual |
|---|---:|
| Packages discovered | 7 |
| Packages finished | 6 |
| Packages failed | 1 |
| Packages aborted | 0 |
| Full tests executed | 0（build gate failed） |

失败 package：`low_speed_av_simulation`。

首个编译根因：

```text
src/low_speed_av_simulation/src/sim_localization_pose_publisher_node.cpp:361:19:
error: nav_msgs::msg::Path has no member named 'clear'
    pose_history_.clear();
```

失败 target：`sim_localization_pose_publisher_node`；`roadnet_visualization_node` 在同 package 内已完成链接。因 full build 失败，按门禁未运行 full colcon test/test-result，不能给出全量测试总数或 PASS。

日志：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/full/log/build_2026-07-12_16-44-48/low_speed_av_simulation/`

## 13. ASan/UBSan

- `ASAN_BUILD_RC=0`：5 packages finished。
- `ASAN_TEST_RC=0`，`ASAN_RESULT_RC=0`。
- Planning 40 PASS、Control 28 PASS、Chassis 7 active PASS；4 Chassis known-gap specs SKIPPED。
- 没有 AddressSanitizer 或 UndefinedBehaviorSanitizer 报告。
- 状态不是 `SANITIZER_ENVIRONMENT_BLOCKED`。

日志：

- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/asan/log/`
- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_20260712_164155/asan/build/*/test_results/`

## 14. GitHub Actions

本机 `gh` 不存在，不能执行给定的 `gh run list`。公开 GitHub Actions 页面确认目标 SHA 的 [ROS2 Humble CI #3 / run 29185828794](https://github.com/yuguangxie/ros2_planning_control/actions/runs/29185828794)：

| Job | Actual |
|---|---|
| `build-test` | FAIL，2m25s，exit 2 |
| `sanitizers` | PASS，3m09s |
| Workflow overall | `EXECUTED_FAIL` |

这是目标 commit 的同 SHA 证据，不使用其他 commit 证明当前提交通过。

## 15. 真实硬件隔离

静态 grep 结果：

- Bringup test 与 workflow 中未发现 `192.168.*`、`keyboard_scu_control` 或 `chassis_driver_node`。
- Chassis core test 中未发现 socket 创建或 `UdpChannel`。

动态执行仅启动 Planning/Control launch tests；没有启动 keyboard control 或 Chassis Driver，没有连接真实 `192.168.x.x`，没有发送 UDP、CAN 或运动命令。未发现 `BLOCKED_REAL_HARDWARE_RISK`。

## 16. Finding 重新判断

| Finding | 当前状态 | Production 证据 | 本次实际测试证据 | 可关闭 | 仍缺证据 |
|---|---|---|---|---|---|
| `CDX-P0-001` | `FIXED_PRODUCTION` | Trajectory emergency/failure 在 controller 前进入 safety state machine；stop bypass 与 SCU brake mapping | safety gtest 6/6 PASS；4×2 matrix PASS；Planning->Control->SCU active launch method PASS | 是，原软件 finding 可关闭 | 整体 launch suite 仍需修复后全绿，不影响该 active 链路本次 PASS 的事实 |
| `CDX-P0-002` | `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION` | Chassis 仍只有 callback 单次发送，无独立 scheduler/watchdog/startup-timeout-shutdown stop | 4 gtest specs + 1 launch spec 均 SKIPPED | 否 | 软件 watchdog 实现与测试；500 ms bench/HIL |
| `CDX-P1-004` | `FIXED_PRODUCTION_TESTED` | 76/76/76 参数合同、fail-fast validator、真实 dt smoother/limiter、独立 rate | config check PASS；pipeline/runtime helper 16/16 PASS；Control runtime active methods PASS | 是 | 实车/闭环性能调参不属于原参数失效 finding 的关闭证据 |
| `CDX-P1-005` | `FIXED_ORIGINAL_FINDING / NEW_SYMLINK_INSTALL_REGRESSION` | absolute/relative/mixed/symlink containment 共用 production helper | loader 19/19 PASS，Linux symlink escape case 实际 PASS | 是（原 escape finding） | 新发现的已安装 sample 与 `--symlink-install` 兼容问题需单独修复/回归 |
| `CDX-P1-006` | `FIXED_PRODUCTION_TEST_BASE` | Planning/Control/Chassis 均注册并链接 production library/core | 75 active C++ cases normal + sanitizer PASS；4 skip 明确不计 | 是（“基本没有 C++ 测试”原 finding） | ROS2 launch 全绿、watchdog 实现属于后续覆盖缺口 |
| `CDX-P1-007` | `OPEN / EXECUTED_FAIL` | workflow 已配置但不能替代执行结果 | 本地 full build FAIL；同 SHA CI run 29185828794 overall FAIL | 否 | 同 SHA full build/test/result 与 required jobs PASS |
| `CDX-P2-003` | `FIXED_PRODUCTION_TESTED` | Loader ID/reference/numeric/index consistency fail closed | loader 19/19 normal + sanitizer PASS | 是 | ROS runtime load success 被新 symlink regression 阻塞，但结构负例已直接验证 |
| `CDX-P2-004` | `FIXED_PRODUCTION_TESTED` | admissible A* scale、weighted 状态、稳定 tie-break、cost rejection | algorithms 13/13 normal + sanitizer PASS | 是 | 无 |
| `CDX-P2-005` | `FIXED_PRODUCTION_UNIT / RUNTIME_BLOCKED` | full reference geometry 重算 summary，terminal helper 共用 production library | helper 8/8 PASS；Planning service runtime 因 package 未加载未形成成功证据 | 否（保守） | 修复 install regression 后 PlanMission/route summary launch PASS |
| `CDX-P2-006` | `FIXED_PRODUCTION_UNIT / PARTIAL_RUNTIME` | Planning 与 Control 均有 identity、单调窗口、heading/gear progress | Planning/Control progress C++ PASS；Control switch runtime PASS；Planning runtime blocked | 否（保守） | Planning loop/progress ROS2 runtime PASS |
| `CDX-P2-007` | `OPEN_CAPABILITY / FAIL_CLOSED_MITIGATION_TESTED` | reverse 专用控制未实现；四 controller 明确 stop | controller/runtime input reverse tests PASS | 否 | reverse 运动学、SIL/HIL；若产品明确不支持 reverse，可另行接受能力边界 |
| `CDX-P2-010` | `FIXED_PRODUCTION_TESTED` | trajectory/controller/model/estop clear reset progress+smoother，READY interlock | runtime helper PASS；四 controller switch/reset launch method PASS | 是 | Planning planner switch runtime 未在本轮单独覆盖 |
| `CDX-P2-011` | `FIXED_PRODUCTION_TESTED` | PARK/unknown 固定 stop，D/R/N 映射明确 | SCU mapper pipeline 11/11 target PASS | 是 | 实车协议确认仍属于 HIL/接口签字边界 |
| `CDX-P3-001` | `FIXED_TESTED` | 默认脚本选择现存正式包 | unified runner/entrypoint regression PASS | 是 | 无 |
| `CDX-P3-002` | `FIXED_TESTED` | 空 parking 返回可解释 skip | entrypoint regression PASS，无 traceback | 是 | 无 |
| `CDX-P3-003` | `FIXED_TESTED` | scripts/config/sample 单一来源关系有自动检查 | template consistency PASS；sample tree hash 相同 | 是 | 无 |

## 17. HIL 边界

硬件 watchdog 保持：

```text
DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED
```

本次没有供应商协议、固件矩阵、CAN 抓包、wheels-off bench、Control/Driver exit、network loss 或 host power loss 故障注入证据。500 ms 声明不得写成 HIL PASS，也不能关闭 `CDX-P0-002`。Control 存活时的主动周期 brake stop 已有软件测试；0x121 完全消失后的实际制动力、停车距离、shift/steering 和恢复策略仍未知。

## 18. 所有首个失败根因

1. 依赖安装（已解决）：初次非交互 sudo 需要密码；Jammy 标准 rosdep 对 package.xml 的 `pytest` key 无定义。交互式 sudo、标准 rosdep 初始化和主机本地 key 映射后，apt/rosdep 全部 RC=0。
2. ROS2 Planning integration：`--symlink-install` 的 sample symlink canonical 到 package root 外，被 containment 拒绝。
3. ROS2 bounded exit tests：Humble `ProcInfoHandler` 没有 `assertWaitForShutdown`。
4. Full build：`nav_msgs::msg::Path` 没有 `clear()`，`low_speed_av_simulation` 编译失败。
5. GitHub Actions：目标 SHA workflow overall Failure；公开页面给出 build-test exit 2，未登录状态不能读取完整远端日志。当前本地 full build 已独立复现 exit 2 级别的编译失败。

## 19. 唯一推荐下一步

执行一个最小 Ubuntu/Humble 验证修复批次：只修复已证实的三个阻断点（installed sample 与 `--symlink-install` containment 兼容、Humble post-shutdown API、Simulation `Path` 清空调用），随后在同一新 commit 重跑本报告的定向 integration、full colcon 和同 SHA GitHub Actions；在三者全部 PASS 前不要进入 HIL 或实车放行。

## 20. 依赖修复后的复验

复验根目录：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217`。

| Revalidation stage | Actual result |
|---|---|
| Directed build | RC=0；6/6 packages finished |
| Planning C++ | 40/40 PASS |
| Control C++ | 28/28 PASS |
| Chassis C++ | 7 active PASS、4 `SKIPPED_KNOWN_PRODUCTION_GAP` |
| Bringup launch | colcon scheduler RC=0，但 `colcon test-result` RC=1；16 methods 为 8 PASS、7 FAIL（含 3 errors）、1 SKIPPED |
| Full build | RC=2；6 packages finished、`low_speed_av_simulation` 失败；full test 未执行 |

依赖升级没有改变 launch 失败：3 个 post-shutdown methods 在 `launch_testing 1.0.14` 下仍报 `ProcInfoHandler` 不存在 `assertWaitForShutdown`；`--symlink-install` sample containment 失败也保持不变。全量构建仍首先失败于 `sim_localization_pose_publisher_node.cpp:361` 的 `pose_history_.clear()`。因此所有剩余阻断均已在依赖完整的系统 Python/ROS2 环境中复现，不能归因于 sudo password、apt 或 rosdep 未完成。

复验日志：

- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217/directed/log/`
- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217/directed/build/*/test_results/`
- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217/directed/ros_log/`
- `/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_deps_fixed_20260712_223217/full/log/`

## 21. Phase 17 本地复验（保留基线失败历史）

本节是基线报告之后的工作区复验，不覆盖第 1—20 节针对 SHA `6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9` 的历史失败事实。Phase 17 当前尚未 commit/push，CI 仍为 `CONFIGURED_NOT_EXECUTED`。

复验根目录：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_phase17_20260712_230907`。

| Gate | Phase 17 local result |
|---|---|
| Standard-only rosdep | PASS；仅 `20-default.list` 时 update/install/check RC=0，无 local mapping |
| Offline | 18/18 PASS |
| Directed build | 7/7 packages PASS |
| Planning C++ | 40/40 PASS；strict containment 生产代码零修改 |
| Control C++ | 28/28 PASS |
| Chassis C++ | 7 active PASS、4 known-gap SKIP |
| Bringup launch | 15 active PASS、1 watchdog known-gap SKIP |
| Simulation reset | build PASS；runtime + exit 2/2 PASS |
| Full build/test/result | 7/7 build PASS；test/result RC=0 |
| ASan/UBSan | 75 active PASS、4 known-gap SKIP；无 sanitizer 错误 |
| GitHub Actions | `CONFIGURED_NOT_EXECUTED`，等待明确 push 授权 |
| HIL | `HIL_NOT_EXECUTED` |

四个原阻断均已在本地解除：Bringup 使用 `python3-pytest`；integration 使用真实物化临时 package root；bundled demo 默认解析可信物理 source root，而用户参数仍走 strict containment；三份 Humble post-shutdown tests 使用 `assertExitCodes`；Simulation reset 以全新 `nav_msgs::msg::Path` 清空 header/poses 后追加 initial pose。

基线失败日志位于 `baseline/`，修复后 directed/full/asan 日志位于同一 Phase 17 root。详细文件清单、安全说明、统计和 finding 判断见 `reports/phase_17_report.md`。

## 22. Phase 18 closed-loop SIL 本地复验

本节继续保留第 1—20 节 baseline 失败和第 21 节 Phase 17 历史，不把未提交工作区结果冒充 SHA `6b07d6c` 的 CI 结果。

复验根目录：`/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_phase18_20260713_085939`。

| Gate | Actual |
|---|---|
| Offline / rosdep | 18/18 PASS；standard metadata check PASS |
| Full build | 7/7 PASS |
| Full test/result | RC=0/0；aggregate 147，0 failure/error，1 known-gap launch skip |
| Simulation C++ | 25/25 production-linked PASS |
| Full SIL | 5 runtime + 1 post-shutdown PASS；四 controller×两车型、goal stop、Planning failure stop、internal-only/no Chassis |
| SIL metrics | lateral RMS/max 0.2745/0.4056 m；goal 0.2855 m/0.0322 rad；stopped 0.0250 m/s；non-finite 0 |
| ASan/UBSan | 100 active production C++ PASS；4 Chassis known-gap SKIP；无 sanitizer error |
| GitHub Actions | `CONFIGURED_NOT_EXECUTED`；工作区未 commit/push，baseline run 仍 `EXECUTED_FAIL` |
| HIL | `HIL_NOT_EXECUTED` |

Phase 18 只建立 kinematic SIL，不启动或修改 Chassis，不验证 500 ms hardware watchdog。详细实现、公式、测试映射、metrics、findings 和 handoff 见 `reports/phase_18_report.md`。
