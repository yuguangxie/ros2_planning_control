# Post Phase 14 Verification and Recommendation

核验日期：2026-07-11  
核验提交：`b1af553b790c22130699258c86920f9586778bad`  
核验分支：`codex/phase-13-control-safety`

## 1. Phase 14 核验结论

Phase 14 已建立真实的 production-linked C++ 测试底座，但报告中的执行状态已经被本次远端 CI 结果更新：它不再是 `CONFIGURED_NOT_EXECUTED`，而是 **`EXECUTED_FAIL`**。

### 基线和实际执行

- 工作区：干净；`git diff --stat` 为空；`git diff --check` PASS。
- 本机：ROS2、colcon、CMake、C++ compiler、pytest 不可用；FreeCAD Python 3.11.14 与 uv 0.10.7 可用。
- 本机 offline runner：17 PASS / 0 FAIL / 0 SKIPPED。
- 本机 repository hygiene：467 个 UTF-8 文本、74 个 JSON、213 个 Markdown PASS。
- 未提交生成物：根目录 `.pytest_cache`、`scripts/__pycache__`、Planning test `__pycache__` 存在但均未被 Git 跟踪；无 build/install/log 被提交。
- GitHub Actions run `29152378189`：总体 `EXECUTED_FAIL`。
  - `build-test`：FAIL。16 个 offline checks 先通过，`repository_hygiene` 因内部 `git ls-files` 返回 128 失败；常规全量 build/test/launch 随后被跳过。
  - `sanitizers`：PASS。ASan/UBSan 下构建 5 个包，执行 Planning、Control、Chassis 三个包的 7 个 CTest targets；44 个 active gtest cases PASS，4 个 watchdog cases `GTEST_SKIP()`，Planning Python wrapper PASS；`colcon test-result` 报告 56 results、0 errors、0 failures。
  - Bringup ROS2 launch test未执行。

### Planning

- `test_roadnet_loader` 与 `test_planning_algorithms` 直接链接 `low_speed_av_planning` production library；没有复制生产算法。
- 19 个 C++ cases 已在 sanitizer job 实际执行并通过。
- RoadnetLoader 有 canonical、1.1.x、validation、blocking、checksum、两种 index 和临时目录坏 fixture。
- graph/motion/speed 有 Dijkstra、A*、blocked/disabled、reverse、拼接、crop、stop-and-wait、curvature 与 obstacle stub。
- 不足：没有 production C++ semantic helper 测试；没有 equal-cost tie-break、负 cost、no-go/speed-zone、semantic terminal/full-reference 等完整矩阵。
- 未修改 Planning 生产算法，没有越界进入第三阶段 loader/A* hardening。

### Control

- `test_safety_state_machine`、`test_controllers`、`test_vehicle_command_pipeline` 均直接链接 `low_speed_av_control` production library。
- 18 个 C++ cases已在 sanitizer job 实际执行并通过。
- Phase 13 emergency/failure、4 controllers x 2 vehicle models、timeouts、VehicleState gates、latch/clear、smoother bypass 与 SCU brake mapping 有直接生产代码测试。
- 不足：Node/topic 级 emergency 回归尚未执行；controller NaN/Inf、非法车型参数、完整 rate/reset/algorithm switch 矩阵仍不完整。

### Chassis

- 已建立 `chassis_driver_core`；`chassis_driver_node` 与 `test_chassis_core` 链接同一 core。
- core 包含 `DbcProtocol`、`CanEthernetCodec`、`ScuControlFrameBuilder`；7 个 active cases 在 sanitizer job PASS。
- DBC source、codec source、UDP source、正式 config 与 launch 相对 Phase 14 前 blob 完全未变；CAN ID 0x121 和 DBC bit mapping 未改变。
- ROS message 到纯 input 的复制仍位于 executable 的 callback；测试直接覆盖 frame builder，不覆盖 ROS callback conversion。
- 没有可复用 transport interface 或 Node-level fake transport。`FakeFrameSink` 只是 test 文件局部容器；`LOW_SPEED_AV_CHASSIS_TEST_MODE` 只存在于 workflow 环境变量，生产代码没有读取它。
- 测试和 CI 不启动 Chassis Driver、keyboard node，不打开 socket，也不包含真实 192.168.x.x 地址，因此没有实际连接真实底盘。
- install/export 和 `ament_cmake_gtest` dependency 已配置，并已由 sanitizer build 验证。

### ROS2 integration

- Bringup 已注册一个带 60 秒 CTest timeout 的 launch test。
- 唯一 active 场景是 invalid goal -> emergency trajectory -> `/control/command` brake -> SCU brake；另有 process exit check。
- Chassis publisher-loss watchdog 用例保持 `SKIPPED_KNOWN_PRODUCTION_GAP`。
- 缺少：canonical ready、PlanRoute/PlanMission success、四 controller runtime switch、localization/trajectory timeout、estop clear、VehicleState gates、late subscriber/QoS、fake Chassis transport。
- 该 launch target 没有在本次远端 CI 执行，因此是 `GENERATED_NOT_EXECUTED`，不能写成 PASS。

### Python、template 与 CI

- runtime/simulation 默认路径已改为正式 `_2`，Windows 与 Ubuntu CI 对应 checks 均 PASS。
- 空 `parking_points` 临时 fixture 在 Windows 与 Ubuntu CI 均 PASS，无未处理 `IndexError`。
- runner 正确聚合 17 项并返回非零失败码；sample、`_1`、`_2` 均覆盖。
- scripts/config/sample/tree consistency 在 Windows 和 Ubuntu CI 均 PASS。
- `ALLOWED_CONFIG_DIFFERENCES` 显式存在但当前为空；当前配置无 package-specific 差异。若未来产生差异，现实现仍需扩展为结构化 allowlist，而不仅是在错误信息中打印字典。
- CI 包含 Humble、rosdep、colcon build/test/result、offline runner、clang-format、ASan/UBSan、环境身份和 artifacts；不启动真实 Chassis。
- CI 缺陷：build-test 在 runner 的 Git 子进程处提前失败，且失败时没有把 `git ls-files` stderr 写入 runner 输出；artifact 因 build/test 未开始而为空。

## 2. Finding 状态

| Finding | 当前状态 | 生产代码证据 | 测试/执行证据 | 是否关闭 | 最小关闭工作 |
|---|---|---|---|---|---|
| CDX-P0-001 | FIXED | Control 保存 trajectory status/emergency，并在 controller 前由 safety machine fail closed | sanitizer job 中 6 个 safety cases、controller/mapper pipeline 实际 PASS；ROS launch 未执行 | 保持关闭 | 在完整 CI 中实际运行 failure trajectory launch test，作为集成证据补强 |
| CDX-P0-002 | OPEN | Chassis callback 构帧后单次发送；主 driver 无 command cache、steady-clock age、scheduler、startup/timeout/shutdown stop 或 diagnostics | 4 个 watchdog gtest 实际 `GTEST_SKIP()`；launch watchdog specification 未执行 | 不可关闭 | 完整生产 watchdog + fake-clock unit tests + fake-transport publisher-loss launch test + 同提交 ROS2/CI 证据 |
| CDX-P1-006 | PARTIALLY_FIXED | Planning/Control/Chassis 已有可链接 production libraries/core | 6 个 production-linked gtest targets、48 C++ cases 已编译；44 active PASS、4 known-gap skip | 暂不关闭 | 补 semantic/helper、非法参数和 ROS conversion/transport 覆盖；完整 CI 通过 |
| CDX-P1-007 | PARTIALLY_FIXED；CI=`EXECUTED_FAIL` | workflow 已配置 | 同提交 sanitizer build/test PASS，但 build-test 失败且 full workspace/launch 未执行 | 不可关闭 | 修复 hygiene Git 调用，执行全量 build/test/test-result 和 launch，保存 artifacts |
| CDX-P3-001 | FIXED | 两脚本默认 `_2` | Windows runner 与 Ubuntu CI 对应 checks PASS | 可以关闭 | 保持 default-entry regression |
| CDX-P3-002 | FIXED | 空列表返回 `SKIPPED_EMPTY_PARKING_POINTS` | Windows/Ubuntu 临时 fixture PASS | 可以关闭 | 保持空列表回归 |
| CDX-P3-003 | FIXED | consistency checker 覆盖 runtime/template config/sample/tree | Windows/Ubuntu consistency check PASS | 可以关闭 | 未来差异必须扩展结构化 allowlist |

## 3. 后续工作优先级

| Priority | Work item | Reason | Dependencies | Modules | Verification | Completion criteria | Risk if deferred |
|---|---|---|---|---|---|---|---|
| P0 | 关闭 CDX-P0-002：Chassis 独立 command watchdog | Control/DDS 消失后仍可能保留旧运动命令 | 当前 core/frame builder；硬件协议确认 | `chassis_driver`、最小 Bringup test/docs | fake-clock gtest、fake-transport launch、CAN frame capture、diagnostics | startup/invalid/timeout/shutdown 均持续 stop；fresh command 周期发送；同提交 ROS2 证据 | 实车失联后无软件 stop 保证 |
| P1 | 修复并闭合完整 CI | 当前 workflow 总体失败，launch 未执行 | 修复 `git ls-files` 128；保持 sanitizer job | scripts、workflow、tests | 全量 build/test/result、artifacts | workflow 所有 required jobs PASS | 回归证据碎片化，错误可能绕过主 job |
| P1 | 完成 ROS2 integration matrix | 当前只有 invalid goal active case | 可用 ROS2 CI、fake transport、watchdog | Bringup、Planning、Control、Chassis test | services/topics/QoS bounded tests | success/failure/timeout/clear/vehicle/QoS 全部 PASS | 节点、QoS、生命周期问题仍不可见 |
| P1 | 补齐 production C++ 覆盖缺口 | semantic/helper、非法参数、ROS conversion 未覆盖 | production helper 可测试边界 | Planning、Control、Chassis | direct-linked gtest | 需求矩阵每项有正负例并在 CI 执行 | Python smoke 与 Node private 逻辑继续漂移 |
| P2 | 工程完整性收口 | fake sink 不是 transport DI；allowlist 仅为空字典 | P0/P1 测试路径稳定 | CMake/package、transport、fixture、templates、docs | export consumer build、cross-platform runner | Ubuntu/Windows 行为一致，差异可审计 | 测试维护成本和配置漂移上升 |
| P3 | Loader/A*/闭环仿真/高级算法/HIL | 属于后续正确性与能力提升 | P0 关闭、P1 CI 绿 | Planning、Simulation、Control、HIL | fuzz/property/SIL/HIL | 各阶段单独 DoD | 过早扩展会掩盖安全和验证基础缺口 |

## 4. 唯一推荐下一任务

任务名称：**Phase 15 — Yunle Chassis 独立命令 watchdog 安全闭环（关闭 CDX-P0-002）**。

推荐原因：这是当前唯一仍开放的 P0 生产安全缺口，直接决定 Control/DDS/publisher 消失后底盘是否收到持续 stop。当前 core 重构已提供 frame builder/codec 测试入口，适合形成独立提交。

不先做其它任务的原因：CI 主 job 失败和 integration coverage 不完整属于 P1；sanitizer job 已证明当前 Chassis core 可以在 Humble 编译测试，足以支持先实现 P0。CI 的最小阻塞修复可作为 watchdog 验证闭环的一部分，但不得扩展成独立工程重写。

允许范围：`src/yunle_chassis/chassis_driver/**`、必要的 Chassis/Bringup test-only launch/config、标准 diagnostics dependency、watchdog 文档、Phase 13/14/final/Phase 15 reports，以及仅为执行 watchdog CI 所需的最小 runner/workflow 修复。

禁止范围：Planning/Control 算法改写、自定义接口字段、CAN ID/DBC bit mapping/现有 topic/service/默认真实网关改变、正式 Roadnet 数据、loader/A*/闭环仿真/高级算法。

完成后解除：`CDX-P0-002`、实车前最关键的软件失联停车阻塞，并使 Phase 13 有条件从 partial 转为 completed；随后才能优先处理完整 CI 和 integration matrix。

## 5. 下一阶段完整 Prompt

````markdown
你现在位于 ROS2 低速自动驾驶项目根目录。

本任务是 Phase 15：实现 Yunle Chassis Driver 独立命令 watchdog 安全闭环，关闭 `CDX-P0-002`。不要实施第三阶段 loader/A*、闭环仿真或高级算法。

# 一、必须先阅读

完整阅读：

- 根目录 `AGENTS.md`
- `docs/audit_cdx/00_AUDIT_INDEX.md`
- `docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `docs/audit_cdx/05_OPTIMIZATION_ROADMAP.md`
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/post_phase_14_recommendations.md`
- `reports/final_generation_report.md`
- `.github/workflows/ros2_humble_ci.yml`
- `scripts/run_offline_checks.py`
- `scripts/offline_repository_hygiene.py`
- `src/yunle_chassis/README.md`
- `src/yunle_chassis/chassis_driver/CMakeLists.txt`
- `src/yunle_chassis/chassis_driver/package.xml`
- `src/yunle_chassis/chassis_driver/config/chassis_driver.yaml`
- `src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py`
- `src/yunle_chassis/chassis_driver/include/chassis_driver/*.hpp`
- `src/yunle_chassis/chassis_driver/src/*.cpp`
- `src/yunle_chassis/chassis_driver/test/test_chassis_core.cpp`
- `src/low_speed_av_bringup/test/test_planning_control_safety_launch.py`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/OPERATOR_STARTUP_CHECKLIST.md`
- `docs/03_ros2_interfaces.md`

文件缺失时记录，不得假定存在。

# 二、当前已知状态

以当前源码重新核验，不得只相信以下摘要：

- 基线提交应包含 Phase 14 commit `b1af553b790c22130699258c86920f9586778bad` 或其后继提交。
- `CDX-P0-001` 已修复；Control 能在 controller 前消费 failure/emergency trajectory 并输出 brake stop。
- `chassis_driver_core` 已包含 `DbcProtocol`、`CanEthernetCodec`、`ScuControlFrameBuilder`；节点和 gtest 链接同一 core。
- 当前 `SCU_Control_Command` callback 仍构帧后立即单次发送。
- 当前主 Chassis Driver 没有命令缓存、steady-clock age、独立 scheduler、startup/timeout/shutdown stop 或 watchdog diagnostics。
- `test_chassis_core.cpp` 中 4 个 watchdog cases 当前为 `GTEST_SKIP()`，不能算通过。
- Phase 14 CI run `29152378189` 总体为 `EXECUTED_FAIL`：sanitizer job 的 production-linked C++ build/test PASS；build-test 在 `offline_repository_hygiene.py` 内 `git ls-files` 返回 128 后失败，full build/test/launch 未执行。
- 当前 `LOW_SPEED_AV_CHASSIS_TEST_MODE` 只在 workflow 中设置，生产代码没有读取；不得把它描述成已有 fake transport。

# 三、目标

在 Chassis Driver 内建立独立于 Control 发布频率和 callback 生命周期的安全发送层：

1. 缓存最新有效 SCU 控制命令及本地接收时间；
2. 使用 `std::chrono::steady_clock` 计算 command age；
3. 以可配置 20–50 Hz 独立 scheduler 周期发送 0x121；
4. 启动后、收到第一条有效命令前持续发送 safe stop；
5. 新命令非法时立即 fail closed，不得继续重放旧运动命令；
6. 最新命令超时后持续发送 safe stop；
7. 正常 shutdown 前 best-effort 发送明确 stop；
8. 提供线程安全状态、diagnostics 和 counters；
9. 用 production-linked gtest、fake clock、fake transport 和 ROS2 launch test 直接验证；
10. 只有生产实现和实际回归证据齐全时才将 `CDX-P0-002` 标为 FIXED。

# 四、允许修改范围

允许修改：

- `src/yunle_chassis/chassis_driver/**`
- Chassis `CMakeLists.txt`、`package.xml`、config、launch、README
- Chassis production core、Node adapter、transport abstraction、test-only fake transport
- `src/low_speed_av_bringup/test/**` 和必要的 test-only launch/config
- 标准 `diagnostic_msgs` dependency
- `.github/workflows/ros2_humble_ci.yml`
- `scripts/offline_repository_hygiene.py`，但只允许修复阻塞同提交 CI 验证的 `git ls-files`/stderr 问题
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/OPERATOR_STARTUP_CHECKLIST.md`
- `docs/07_config_launch_runtime.md`
- `docs/03_ros2_interfaces.md`，仅在 diagnostics/topic 合同确实变化时更新
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/final_generation_report.md`
- 新增 `reports/phase_15_report.md`

# 五、禁止修改范围

- 不修改 Planning、Control 算法和职责边界；除 test-only integration wiring 外不改其生产代码。
- 不修改现有自定义 msg/srv 字段，不为 watchdog 发明私有接口；diagnostics 优先使用 `diagnostic_msgs/msg/DiagnosticArray`。
- 不改变现有 ROS topic/service 名称。
- 不改变 CAN ID、DBC signal 名称、start bit、length、factor、offset、sign、byte order 或 shift/steering/speed/brake 生产映射。
- 不改变正式节点默认真实网关地址、端口和现有车辆协议。
- 不修改正式 Roadnet 包或 canonical AD Package 合同。
- 不实施 manifest path hardening、A* 修正、loader hardening、闭环仿真、高级 Planning/Control 或 HIL 放行。
- 不把 fake transport、接口骨架、日志字符串、token check、`GTEST_SKIP()` 或未执行测试当成 watchdog 已实现。

# 六、硬约束

1. Driver watchdog 必须独立于 Control 持续发布 stop。
2. 安全 stop 优先于缓存的运动命令、normal frame builder 和任何 limiter。
3. command age 必须使用 `std::chrono::steady_clock`，不能依赖 ROS time、header stamp 或 system clock。
4. scheduler 默认频率必须处于 20–50 Hz，参数必须有限且大于 0。
5. timeout 必须有限且大于 0；建议默认小于数个 scheduler 周期总和并按当前低速项目保守设置。
6. startup、invalid、timeout、shutdown stop 必须固定为 target speed 0、front/rear steering 0、brake enable true，并使用合法 stop shift。
7. 非法新命令必须使旧运动缓存立即失效；不能简单丢弃后继续重放旧命令。
8. scheduler、callback、shutdown、diagnostics 和 UDP TX 的共享状态必须无 data race、死锁或 use-after-free。
9. destructor/shutdown 路径不得抛异常；stop 是 best-effort，失败必须计数和记录。
10. 进程硬崩溃、断电、网关/CAN 断链仍必须依赖硬件 watchdog；软件不得声称覆盖。
11. 测试默认不得打开真实 socket、访问 `192.168.x.x` 或启动 keyboard node。
12. 时间测试使用 fake clock/显式 time point，不以真实 sleep 作为主要断言机制。
13. 保留用户已有改动，不 reset、clean、删除历史报告或 fixture。
14. 无 ROS2 时不得伪造 build/test PASS。

# 七、开始前基线

执行并记录：

- `git status --short`
- `git diff --stat`
- `git diff --check`
- 当前 commit SHA、branch、remote tracking
- `ros2`、`colcon`、`cmake`、`cl`、`g++`、`clang++`、pytest、uv、Python 可用性
- build/install/log/cache 是否被提交或误生成
- 当前 Chassis constructor、callback、send、RX threads、destructor 生命周期
- 当前 CAN 0x121 frame builder 和 DBC mapping
- 当前 CMake target/link/install/export 和 package dependencies
- 当前 watchdog `GTEST_SKIP()` cases
- 当前 CI run 状态和失败日志；不得沿用 `CONFIGURED_NOT_EXECUTED`

保存修改前 offline runner、Chassis gtest/CI 可用结果作为基线。

# 八、生产实现要求

## 8.1 可测试 watchdog core

在 `chassis_driver_core` 中新增 ROS-independent watchdog/scheduler decision 类，例如 `ScuCommandWatchdog`，名称可按现有规范调整。

至少具有：

- `submit_valid_command(command, receive_time)`
- `submit_invalid_command(reason, receive_time)` 或同等强度的 invalidation API
- `decision(now)`，返回发送 fresh command 或 safe stop
- `startup`、`fresh`、`invalid`、`timeout`、`shutdown` 等明确状态/reason
- command age、watchdog active、last reason、valid-cache 状态
- 可由测试显式传入 steady-clock time point

不得把 ROS Node clock、timer 或 UDP 直接塞进 pure decision 类。

## 8.2 callback 与缓存

- ROS subscription callback 只负责消息转换、完整校验并提交缓存。
- 必须校验 shift、NaN/Inf、速度/转角范围和所有直接参与 0x121 的 flags。
- 有效命令记录 `steady_clock::now()` receive time。
- 无效命令立即进入 invalid stop，并清除/屏蔽更早的运动命令。
- callback 不再作为运动 frame 的唯一发送时机；是否立即发送可选，但不能替代 scheduler。

## 8.3 独立 scheduler

- 使用 ROS wall timer 或独立安全线程，以配置频率周期运行。
- 默认频率建议 20–50 Hz 范围内的保守值。
- 每周期调用 watchdog decision：fresh 时发送缓存命令，startup/invalid/timeout 时发送 stop。
- timeout 后每个周期持续发送 stop，不能只发送一次。
- scheduler 与 RX threads 生命周期清晰；构造失败和部分初始化失败必须 fail closed。

## 8.4 safe stop frame

- 复用 production frame builder/DBC，不复制 bit mapping。
- target speed = 0。
- front/rear steering = 0。
- brake enable = true。
- stop shift 为可配置合法值，非法参数启动失败或统一回退安全默认；策略写入文档。
- safety flags 使用明确固定值，不继承旧运动命令的灯光/valid flags，除非协议文档有明确理由。

## 8.5 shutdown

- 正常 `rclcpp::shutdown`、Node destructor 和 worker stop 顺序中，在关闭 transport 前 best-effort 发送 stop。
- scheduler 必须先停止产生新运动帧，再发送 shutdown stop，再关闭 UDP/RX threads。
- destructor 不得抛异常；失败增加 counter、日志和 diagnostics last state。

## 8.6 transport abstraction 与 fake transport

- 为 TX 建立薄接口/依赖注入，使 Node/executable 使用真实 UDP adapter，测试使用 in-memory fake。
- fake transport 必须能记录 frame、timestamp、channel、success/failure；不得打开 socket。
- production 默认仍使用真实 UDP 和现有地址。
- 若增加 `transport.mode` 或 test-only 参数，默认必须为 production UDP，且只允许显式 test launch 使用 fake。
- 删除或真正实现当前无效的 `LOW_SPEED_AV_CHASSIS_TEST_MODE` 语义；不得保留让人误以为已隔离的空环境变量。

## 8.7 diagnostics 和 counters

使用标准 `diagnostic_msgs/msg/DiagnosticArray`，至少报告：

- scheduler state
- last command age
- watchdog active
- last stop reason
- cached command valid
- TX attempt/success/failure counts
- startup/invalid/timeout/shutdown stop counts
- CAN1/CAN2 open/channel state
- last TX result

diagnostics 不得反向依赖 `low_speed_av_control`。

## 8.8 参数

至少新增并校验：

- `scu_control_publish_rate_hz`
- `scu_control_command_timeout_s`
- `scu_control_startup_stop_enabled`
- `scu_control_shutdown_stop_enabled`
- `scu_control_stop_shift_level`
- diagnostics publish rate/topic（若需要）
- transport mode（若采用）

rate/timeout/limit 必须 finite；rate/timeout > 0；stop shift 合法。非法安全参数不得静默进入未定义行为。

# 九、测试要求

## 9.1 production-linked C++ gtest

直接链接 `chassis_driver_core`，启用或替换现有 skipped watchdog cases。至少覆盖：

- startup 无命令 -> 每周期 stop
- fresh valid command -> 每周期运动 frame
- fresh command 的 age 边界
- timeout 前仍 fresh
- timeout 到达/超过 -> 持续 stop
- invalid shift -> 立即 stop
- NaN/Inf -> 立即 stop
- overrange 按明确策略 fail closed
- invalid new command 不重放旧运动 command
- 后续 fresh valid command 按定义恢复
- shutdown -> 明确 stop
- stop frame 0x121 bit 精确值
- fake transport success/failure counters
- diagnostics snapshot/state/reason/counters
- scheduler 决策确定性
- 并发/生命周期可测试边界

所有时间用 fake `steady_clock::time_point`；不得依赖长 sleep。每个测试只验证一个主要行为。

## 9.2 ROS2 launch/integration

新增/扩展 bounded launch test：

1. 使用 fake transport 启动 Chassis Driver；
2. 启动后未发布命令，观察周期 startup stop；
3. 发布 fresh motion command，观察周期 motion frame；
4. 停止 publisher，超过 timeout 后观察连续 stop frame；
5. 发布非法新命令，确认不再重放旧 motion；
6. shutdown 前观察 stop 或从 fake transport lifecycle evidence 断言；
7. diagnostics 报告 watchdog active/reason/counters；
8. 测试有总 timeout，失败打印收到的 frames、age、reason 和 counters。

把现有 `SKIPPED_KNOWN_PRODUCTION_GAP` publisher-loss specification 转为 active test，前提是生产实现真实存在。不得仅删除 skip。

## 9.3 网络隔离

- CI/test source 中不得出现真实 192.168.x.x 地址。
- 不启动 keyboard node。
- 不使用真实 UDP gateway。
- loopback 测试如确有必要，只能 `127.0.0.1` + 动态端口；优先 pure fake transport。

# 十、ROS2 与 CI 执行规则

## ROS2/colcon 可用

执行：

```bash
colcon build --symlink-install --packages-up-to chassis_driver low_speed_av_bringup
colcon test --packages-select chassis_driver low_speed_av_bringup --event-handlers console_direct+
colcon test-result --verbose
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
python3 scripts/run_offline_checks.py
```

记录 package、CTest target、gtest case、PASS/FAIL/SKIPPED 数和 artifacts。

## ROS2/colcon 不可用

- 运行统一 offline runner、template/repository hygiene、JSON/UTF-8/Markdown link、CMake/package static consistency 和 `git diff --check`。
- C++ 源码标 `GENERATED_NOT_EXECUTED`。
- launch test 标 `SKIPPED_ROS2_UNAVAILABLE`。
- 不得声称 FIXED 或 PASS。

## CI

- 修复当前 `repository_hygiene.py` 调用 `git ls-files` 返回 128 的问题，并把 stderr 输出到失败日志；只做该验证阻塞所需的最小修改。
- 确保 container checkout 的 Git safe-directory/working-directory 行为明确。
- 运行 full build-test 与 sanitizer jobs。
- 上传 test-result/log artifacts。
- workflow 所有 required jobs 都成功前，CI 状态为 `EXECUTED_FAIL`。
- sanitizer job 单独 PASS 不能替代 full workspace/launch PASS。

# 十一、文档和报告

更新：

- `src/yunle_chassis/README.md`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/OPERATOR_STARTUP_CHECKLIST.md`
- `docs/07_config_launch_runtime.md`
- `docs/03_ros2_interfaces.md`（仅真实接口合同变化）
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/final_generation_report.md`
- 新增 `reports/phase_15_report.md`

文档必须说明：

- Control watchdog 与 Chassis watchdog 的职责区别；
- startup/invalid/timeout/shutdown stop 语义；
- scheduler rate、timeout、stop shift 和 diagnostics；
- fake transport 只用于测试；
- 进程硬崩溃、断电和链路故障仍依赖硬件 watchdog；
- sim/bench/vehicle 安全边界；
- observed、expected、generated-not-executed、skipped 的区别。

`phase_15_report.md` 至少包含：

- Phase status
- Goal
- Baseline commit/branch/environment
- Files changed
- Architecture and lifecycle
- Parameters and compatibility
- Production/test target mapping
- Tests actually run
- CI run URL/SHA/status
- PASS/FAIL/SKIPPED/GENERATED_NOT_EXECUTED counts
- CDX-P0-002 evidence and state
- Hardware watchdog contract
- Known limitations
- Next handoff

# 十二、Finding 状态规则

- `CDX-P0-001`：只核验不重写；若被破坏则重新打开。
- `CDX-P0-002`：默认 OPEN。
- 只有以下全部满足才可标 FIXED：
  1. production command cache 存在；
  2. production steady-clock age 存在；
  3. production 20–50 Hz scheduler 存在；
  4. startup/invalid/timeout/shutdown stop 进入真实执行路径；
  5. diagnostics/counters 存在；
  6. active production-linked gtest 全部通过；
  7. publisher disappearance fake-transport launch test 实际通过；
  8. 同提交 ROS2/CI 证据完整；
  9. 4 个原 skipped specs 不再依赖 `GTEST_SKIP()`；
  10. 文档明确硬件 watchdog 边界。
- 若生产已实现但 ROS2/CI 未执行，使用 `PARTIALLY_FIXED / GENERATED_NOT_EXECUTED`，不得写 FIXED。
- `CDX-P1-007` 只有 full workflow PASS 才能关闭；当前起点为 `PARTIALLY_FIXED / EXECUTED_FAIL`。

# 十三、完成标准

只有同时满足以下条件任务才完成：

1. callback 不再是运动 frame 的唯一发送路径；
2. latest valid command 和 receive time 被线程安全缓存；
3. command age 使用 steady clock；
4. scheduler 独立周期发送；
5. startup 持续 stop；
6. invalid new command 立即 stop 且不重放旧命令；
7. timeout 持续 stop；
8. shutdown best-effort stop；
9. stop frame 字段固定且 DBC mapping 未改变；
10. diagnostics/counters 可观察；
11. fake clock、fake transport、production-linked gtest 完整；
12. publisher-loss launch test 为 active 且有 timeout；
13. 测试/CI 不连接真实底盘；
14. 当前环境所有可运行检查通过；
15. 不可运行项诚实标记；
16. CI 的 hygiene 失败被最小修复并有同提交结果；
17. `git diff --check` PASS；
18. 无 build/install/log/cache 或正式 fixture 被误提交；
19. 未进入禁止范围；
20. 只有证据齐全时 `CDX-P0-002=FIXED`。

# 十四、最终回复格式

最终回复只包含：

1. 实际生产修改；
2. watchdog 状态机和生命周期；
3. production/test targets；
4. 实际执行命令；
5. PASS/FAIL/SKIPPED/GENERATED_NOT_EXECUTED；
6. CI 实际状态和 URL；
7. `CDX-P0-002` 最终状态及证据；
8. 硬件 watchdog 剩余风险。

不要把未执行测试写成 PASS，不要自动开始后续阶段。

# 十五、明确停止范围

完成上述 watchdog 安全闭环后停止。不要继续实施：

- manifest path/symlink hardening；
- A* 修正；
- loader schema/重复 ID/负 cost 扩展；
- 闭环仿真；
- Frenet/Hybrid A*/高级 MPC；
- 与 CDX-P0-002 无关的生产重写；
- HIL 或实车放行。
````

## 6. 剩余风险

1. `CDX-P0-002` 仍为 OPEN，当前 driver 对 publisher 消失没有软件级周期 stop 保证。
2. 远端 CI 总体失败，常规 full workspace build/test 和 Bringup launch 未执行；Phase 14 原报告的 `CONFIGURED_NOT_EXECUTED` 已过时。
3. fake sink 不是 transport dependency injection；当前 Chassis Node 一构造就打开真实 UDP，因此不能安全用于 integration test。
4. Planning semantic/helper C++ 覆盖和 ROS2 integration matrix 明显不完整。
5. sanitizer job 的 PASS 证明当前 selected production targets 可构建测试，但不能证明真实 UDP、CAN gateway、硬件 watchdog、制动距离或实车安全。
