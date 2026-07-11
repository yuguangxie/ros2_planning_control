# 第二阶段完整 Prompt：C++/ROS2 测试底座与 CI

下面的 Prompt 假设第一阶段安全闭环已经完成。它可直接复制到新的 Codex 任务中，用于建立真正执行生产 C++ 的测试体系，关闭 CDX-P1-006、CDX-P1-007，并处理与测试入口直接相关的 CDX-P3-001、CDX-P3-002、CDX-P3-003。

```text
你现在位于 ROS2 低速自动驾驶项目根目录。请先完整阅读根目录 AGENTS.md，以及：

docs/audit_cdx/00_AUDIT_INDEX.md
docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md
docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md
docs/audit_cdx/05_OPTIMIZATION_ROADMAP.md
docs/audit_cdx/07_PHASE_01_SAFETY_OPTIMIZATION_PROMPT.md
reports/phase_13_report.md

先检查第一阶段实际代码和报告，不要只假设 Prompt 已执行。如果 CDX-P0-001 或 CDX-P0-002 仍是 OPEN，先停止扩大测试范围，在报告中把阶段二标为 BLOCKED_BY_PHASE_13，并指出缺失项；不要用测试文档掩盖未完成的安全代码。

本任务是第二阶段优化：建立直接验证生产 C++/ROS2 实现的自动化测试底座和 CI，关闭：

- CDX-P1-006：生产 C++ 基本没有自动化测试。
- CDX-P1-007：当前 commit 缺少同等级 ROS2 回归证据。
- CDX-P3-001：offline_runtime_followup_smoke.py 和 offline_simulation_smoke.py 默认包路径失效。
- CDX-P3-002：offline_algorithm_smoke.py 遇到空 parking_points 直接 IndexError。
- CDX-P3-003：template 与当前运行脚本/配置漂移且没有同步检查。

目标是让测试真正链接并执行 RoadnetLoader、Planner、Controller、Safety、SCU Mapper、DBC/Codec 等生产代码，而不是继续以 Python 复刻或源码 token 搜索代替。

## 一、硬约束

1. 保持现有 ROS2 package 边界，不把所有实现搬进一个测试包。
2. C++ 单元测试必须链接生产 library/target，不复制一份算法到 test 文件。
3. Python smoke 继续保留为数据合同、fixture 和无 ROS2快速检查，但不得被描述成 C++ 行为证明。
4. 不修改正式 Roadnet 包来适配测试；需要特殊坏包/语义点时，在临时目录生成 fixture。
5. 不引入重型第三方测试框架；优先 ament_cmake_gtest、ament_cmake_pytest、launch_testing。
6. 当前环境没有 ROS2 时，仍要生成正确的 CMake/package/test 源，运行可用的 Python 检查，并将 ROS2/C++ 执行标为 SKIPPED_ROS2_UNAVAILABLE。
7. 不声称 CI 配置文件存在就等于 CI 已通过。
8. 保留用户已有改动，不重置工作区，不删除历史报告。
9. 测试必须确定性、无真实网络/底盘依赖、默认不发送到真实 192.168.x.x 设备。
10. 所有测试默认使用 loopback、mock transport、临时目录或纯函数。

## 二、开始前基线

执行并记录：

- git status --short
- 当前 commit SHA（如果仓库有 commit）
- ros2、colcon、cmake、C++ compiler、pytest 可用性
- 当前所有 scripts/*.py 默认运行结果
- sample、正式包 _1、正式包 _2 validator 结果
- 当前 CMake 中 BUILD_TESTING、ament test 注册情况
- 当前 source library target 与 executable target 关系

输出一个测试覆盖矩阵，至少包含：模块、生产 target、计划 test target、运行环境、正例、负例、是否安全关键。

## 三、重构可测试 target，但不改变运行行为

当前 Planning 和 Control 已有 library，可直接链接时优先复用。不要为了测试重复编译不同实现。

对 chassis_driver 做最小必要重构：

- 将 DbcProtocol、CanEthernetCodec、watchdog decision、SCU frame builder 等无 ROS 或低 ROS 依赖逻辑形成可链接的 core library target。
- executable 继续链接同一 core library。
- UDP 实际 send/receive 使用薄适配层；测试使用 fake/mock transport 或只测试 codec/frame builder，不绑定真实网络。
- 不因重构改变 topic、CAN ID、bit mapping、默认参数或线程生命周期。

如果某类逻辑仍深埋 Node private callback，应提取为纯类/函数；ROS node 只负责消息转换、clock 和 publish/subscribe。

## 四、Planning C++ gtest

在 low_speed_av_planning 注册 ament_cmake_gtest，至少覆盖：

### RoadnetLoader

- canonical sample load 成功；
- schema 不是 low_speed_roadnet_ad_package 时拒绝；
- 1.1.0 与兼容 1.1.x；
- manifest validation failed；
- blocking_errors > 0；
- validation report failed；
- checksum mismatch；
- checksums 与 manifest hashes 冲突；
- end_index_exclusive；
- legacy inclusive end_index；
- 篡改 waypoint/index 后拒绝；
- manifest absolute path、../ path、symlink escape（若第一阶段/后续已实现 containment）；
- 结构错误使用临时 fixture，不污染 sample。

### TopologyGraph / Dijkstra / A*

- 正常最短路；
- 不可达；
- disabled/blocked edge；
- reverse disabled；
- start == goal；
- 等价 cost 的确定性；
- A* 与 Dijkstra 在 admissible 配置下结果/总代价一致；
- 非法负 cost 必须被 loader 拒绝或 planner 明确拒绝。

### Motion/Speed

- 多 edge waypoint 拼接；
- end_index 两种格式；
- 边界去重；
- route_s_m 单调重算；
- horizon crop；
- stop_and_wait 全零速；
- curvature speed 有限且不超过 editor/max speed；
- no-go/speed-zone 生效；
- obstacle_aware 明确按当前 stub 合同测试，不能假装有障碍物系统。

### Semantic/trajectory helper

如果相关逻辑仍在 PlanningNode private 中，先提取纯 C++ helper，再测试：

- current pose anchor；
- task/parking/charging linked_node 和 linked_edge fallback；
- null/"null" 不作为 node id；
- same edge forward；
- reverse disabled；
- final semantic stop；
- trajectory continuity 与最大 jump；
- full reference/local trajectory 的 s 和终点一致性。

## 五、Control C++ gtest

在 low_speed_av_control 注册 ament_cmake_gtest，直接链接生产 target，至少覆盖：

### Controllers

- Pure Pursuit、Stanley、LQR、MPC sampler 正常有限输出；
- 空轨迹安全 stop；
- 单点/终点轨迹；
- NaN/Inf 输入 fail closed；
- 低速/零速；
- gear 方向合同；
- 相同输入输出确定性。

### Vehicle models

- front Ackermann 公式；
- dual Ackermann counter-phase 公式；
- 前后轮限幅；
- 非法 wheelbase/ratio 的安全行为。

### Limiter/Smoother

- speed、accel、decel、front/rear steering limit；
- front/rear steering rate；
- emergency bypass；
- 连续多个周期；
- 非法参数 fail-fast；
- reset/algorithm switch 后状态明确。

### Safety state machine

直接测试第一阶段生产状态机：

- WAIT_INPUTS -> READY -> ACTIVE；
- trajectory emergency/failure/invalid；
- localization/trajectory/vehicle state timeout；
- autonomous disabled、brake pressed、fault；
- estop latch；
- 普通 ok 不 clear；
- explicit clear success/failure preconditions；
- stop reason priority；
- 新合法 trajectory 如何恢复；
- 4 controllers x 2 vehicle models 的 emergency matrix。

### ScuCommandMapper

- m/s 到 km/h；rad 到 deg；
- D/R/N/PARK/unknown gear 策略；
- front/rear sign；
- clamp/zero policy；
- emergency、controlled stop、disable、brake；
- NaN/Inf；
- stop command 固定字段。

## 六、Chassis C++ gtest

至少覆盖：

### DBC/Codec

- 13-byte encode/decode round trip；
- standard/extended ID；
- DLC 边界；
- trailing bytes；
- Intel/Motorola、signed/unsigned signal；
- 每个当前支持 CAN message 的关键边界值；
- 0x121 shift、front/rear steering、speed、brake 和 flags bit 精确映射。

### SCU frame builder/watchdog

- 合法 command frame；
- 非法 shift/NaN/越界 fail closed；
- startup stop；
- fresh command；
- timeout stop；
- invalid new command 不继续旧运动 command；
- scheduler 周期决策；
- shutdown stop frame；
- diagnostics counter/state。

测试不得打开真实 UDP socket。UdpChannel 的 loopback 集成测试如实现，必须绑定 127.0.0.1 和动态端口，且默认可重复运行。

## 七、ROS2 launch/integration tests

使用 launch_testing 或等价 ament 测试，至少准备以下端到端用例：

1. Planning 加载 canonical sample 并发布 ready RoadnetStatus。
2. PlanRoute/PlanMission 成功，收到 GlobalRoute 和 Trajectory。
3. 无效 goal 发布 failure status + emergency trajectory。
4. failure trajectory 进入 Control 后，/control/command 与 SCU 都是 brake stop。
5. 依次切换四种 controller，emergency 行为不变。
6. localization timeout 与 trajectory timeout。
7. safety estop、普通 ok 不 clear、clear service precondition。
8. vehicle state disabled/fault/brake。
9. chassis command publisher 停止后 watchdog stop；使用 fake transport/测试模式，不接真实网关。
10. late subscriber 能获取/等待必要状态，QoS 行为有确定断言。

测试必须有超时，失败时打印 topic/service/state 诊断，不允许永久挂起。

如果当前环境没有 ROS2，只生成这些测试并在报告中标记未执行。不要用 Python replica 代替它们并宣称 launch test 通过。

## 八、修复现有 Python 测试入口

修复：

- scripts/offline_runtime_followup_smoke.py
- scripts/offline_simulation_smoke.py

其默认 package 不得再指向不存在的 roadnet_ad_package_20260610T012525Z。可以选择：

- 默认使用明确存在的 _2；或
- package 参数改为必填并给出清晰 usage；或
- 自动发现但必须在多个候选时确定性选择并打印选择结果。

修复 scripts/offline_algorithm_smoke.py：

- 明确它是 sample fixture test，默认只使用 bringup sample；或
- 对空 parking_points 给出可解释 skip/fallback；
- 不允许 IndexError traceback 作为正常行为。

新增一个统一入口，例如 scripts/run_offline_checks.ps1 或跨平台 Python runner：

- 自动寻找可用 Python；
- 逐项运行全部 offline checks；
- 输出每项 PASS/FAIL/SKIPPED 和最终非零退出码；
- 不吞 stderr；
- 正确传入 sample/_1/_2；
- 不修改 Roadnet fixture。

## 九、Template 同步检查

为以下复制关系建立明确策略：

- templates/offline_validation vs scripts
- templates/sample_config vs src package config
- templates/sample_ad_package vs bringup sample

选择一个 canonical source。可以生成复制，也可以在 CI 做差异检查；但不能继续无约束手工维护。

新增测试至少能检测：

- 必要接口/PlanMission/Simulation/Chassis 文件未进入 expected tree；
- template validator 落后于运行脚本；
- sample AD Package hash 漂移；
- config key 在 template 与生产配置中不一致。

不要简单要求所有配置字节完全相同；允许 package-specific 差异，但必须显式列出允许差异。

## 十、CI

如果仓库使用 GitHub，新增 .github/workflows/ros2_humble_ci.yml；若已有其他 CI 平台，遵循现有平台，不要重复建设。

CI 至少包含：

1. Ubuntu + ROS2 Humble 环境。
2. rosdep install --from-paths src --ignore-src -r -y。
3. colcon build --symlink-install。
4. colcon test --event-handlers console_direct+。
5. colcon test-result --verbose。
6. offline check runner。
7. clang-format/ament lint（按仓库可用性）。
8. 独立或可选 ASan/UBSan C++ job，至少覆盖纯 C++ tests。
9. 上传 test-result/log artifact，便于追踪失败。

CI 不得访问真实底盘 IP，不得启动 keyboard control，不得发送运动 CAN。

将当前 commit SHA、ROS distro、compiler、依赖安装结果写入 CI 日志。若 CI 无法在本地触发，报告状态必须写 CONFIGURED_NOT_EXECUTED，而不是 PASS。

## 十一、CMake/package.xml 要求

按包增加必要 test dependencies：

- ament_cmake_gtest
- ament_cmake_pytest
- launch_testing / launch_testing_ament_cmake（确实使用时）
- 其它标准消息依赖

所有测试注册放在 if(BUILD_TESTING) 下。生产库正确 export include、target、dependency。避免把 node main 编进 library，避免测试重复 main。

Chassis core library 重构后，install/export 规则必须完整，不破坏 chassis_driver_node 与 keyboard_scu_control_node。

## 十二、测试命名和可维护性

测试名称包含模块和行为，例如：

test_roadnet_loader_checksum_mismatch
test_control_safety_emergency_trajectory
test_scu_mapper_emergency_stop
test_chassis_watchdog_timeout_stop

每个测试只验证一个主要行为；共享 fixture 放入 test/support，不复制大段 JSON/YAML。随机测试必须固定 seed 并打印 seed。

所有时间相关测试使用 fake clock/显式 timestamp 或足够稳定的 bounded wait，避免依赖 sleep 和机器负载。

## 十三、文档

更新：

- docs/08_testing_without_ros2.md
- docs/ROS2_INTEGRATION_TEST_PLAN.md
- README.md 测试章节
- 各 package README 测试章节
- docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md 中对应问题状态

新增或更新测试矩阵文档，说明：

- 哪些是 C++ unit；
- 哪些是 ROS2 integration；
- 哪些是 Python data contract；
- 哪些需要 HIL/真实底盘；
- 当前实际执行环境和结果；
- 如何复现单个失败测试。

不要删除历史 Ubuntu 报告；新结果必须绑定当前 commit 和日期，并区分 observed result 与 expected procedure。

## 十四、实际执行规则

检测环境后执行最大可行集合：

### ROS2/colcon 可用

运行：

colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose

必要时先定向 packages-select 调试，但最终必须全量执行。保存准确的 package/test 数量和失败详情。

### ROS2/colcon 不可用

- 运行统一 offline runner。
- 运行 JSON/UTF-8/Markdown link/config consistency 检查。
- 不执行 colcon。
- 报告逐条写 SKIPPED_ROS2_UNAVAILABLE。
- 明确 C++ test source 是 GENERATED_NOT_EXECUTED。

无论哪种环境都运行 git diff --check，并确认测试没有生成被错误提交的 build/install/log/cache。

## 十五、阶段报告与最终报告

创建：

reports/phase_14_report.md

包含：

# Phase 14 Report
- Goal
- Files changed
- Key design decisions
- AD Package compatibility notes
- Config/topic compatibility notes
- Tests or offline checks run
- ROS2 commands skipped because ROS2 is unavailable
- Known limitations
- Next phase handoff

额外必须包含：

- 测试目标与生产 target 映射表；
- test case 数量和实际执行数量；
- PASS/FAIL/SKIPPED/GENERATED_NOT_EXECUTED；
- 当前 commit SHA；
- CDX-P1-006、CDX-P1-007、CDX-P3-001、CDX-P3-002、CDX-P3-003 的状态与证据；
- CI 状态：EXECUTED_PASS、EXECUTED_FAIL 或 CONFIGURED_NOT_EXECUTED。

同时更新 reports/final_generation_report.md：保留历史内容，增加“审计后两阶段优化状态”章节，不得把未运行的 ROS2/C++ 测试改写为成功。

## 十六、完成标准

只有满足以下条件才能将第二阶段标为完成：

- Planning、Control、Chassis 均注册直接链接生产 C++ 的 gtest。
- 第一阶段的 emergency trajectory 和 chassis watchdog 有生产代码级回归测试。
- 至少有 Planning -> Control -> SCU 的 ROS2 integration test source。
- Python smoke 默认命令不再因失效路径失败。
- 空 parking_points 不再触发未处理 IndexError。
- template/config/sample 漂移有自动检测。
- CI 能构建、测试并保存结果；若未执行，状态被诚实记录。
- 当前环境所有可运行检查通过，不能运行的项目明确 SKIPPED。
- 文档清楚区分 Python replica、C++ unit、ROS2 integration 和 HIL。

完成后停止，不继续实施第三阶段 manifest path hardening、A* 修正、闭环仿真或高级算法。最终回复只汇报实际完成内容、测试证据、未执行项、剩余风险和第三阶段交接建议。
```
