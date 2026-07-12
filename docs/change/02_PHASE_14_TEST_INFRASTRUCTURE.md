# Phase 14：生产代码测试底座与CI

## 1. 阶段定位

Phase 14解决“代码已经存在，但缺少直接验证生产C++的自动化测试”问题。目标不是继续增加Python算法复刻，而是让gtest链接与节点相同的production library/core，并为ROS2 integration、统一离线入口、模板同步和CI建立可持续底座。

阶段状态为`IMPLEMENTED_WITH_ACCEPTED_PHASE_13_GAP`。实施时获得明确例外：即使`CDX-P0-002`仍开放，也继续建设测试基础设施；这不等于接受或关闭Chassis软件watchdog缺口。

## 2. Production target可测试化

### 2.1 Planning

Planning测试直接链接`low_speed_av_planning` production library，主要建立：

- `test_roadnet_loader`；
- `test_planning_algorithms`。

覆盖canonical包、schema/version、validation、checksum、两种end index、Dijkstra/A*、路径拼接、crop和speed planning等基础行为。

具体作用是让Loader和规划算法的回归失败直接反映生产实现，而不是只验证Python中重新写的一套逻辑。

### 2.2 Control

Control在原安全状态机测试之外增加：

- `test_controllers`；
- `test_vehicle_command_pipeline`。

覆盖四种controller、两种vehicle model、limiter/smoother和ScuCommandMapper。测试target链接`low_speed_av_control`，没有复制controller或mapper实现。

### 2.3 Yunle Chassis最小可测试性重构

Phase 14历史上对`src/yunle_chassis`进行了允许范围内的最小重构：

- 建立`chassis_driver_core`；
- 将`DbcProtocol`、`CanEthernetCodec`和`ScuControlFrameBuilder`放入可链接core；
- `chassis_driver_node`和`test_chassis_core`链接同一个core；
- frame测试使用内存fake sink，不打开真实UDP；
- 保持CAN ID 0x121、DBC bit mapping、topic、网关地址和callback单次发送语义。

具体作用是使DBC bit、13-byte codec和0x121 frame构造能够脱离真实底盘直接回归，同时保证测试与生产节点使用同一实现。

这次重构没有实现：

- 命令缓存；
- steady-clock command age；
- 周期scheduler；
- startup/timeout/shutdown stop；
- watchdog diagnostics。

相关watchdog用例只作为`GTEST_SKIP()`或specification保留，状态是`SKIPPED_KNOWN_PRODUCTION_GAP`，不计入PASS。

## 3. ROS2 Integration测试源码

新增Bringup launch test，验证：

```text
Planning invalid goal
  -> failure status + emergency trajectory
  -> Control brake stop
  -> /control/command 与 SCU command均为零速制动
```

测试带超时、进程退出检查和失败诊断，不启动真实Chassis网络，不访问真实网关，也不启动keyboard control。

具体作用是把Phase 13的安全合同从单类测试提升到Planning与Control节点之间的消息级回归入口。

## 4. Python入口修复

Phase 14修复了三个审计问题：

### 4.1 失效默认包路径

`offline_runtime_followup_smoke.py`与`offline_simulation_smoke.py`不再默认指向不存在的无后缀Roadnet目录，而是确定性使用现存正式包或由runner显式传入。

### 4.2 空parking_points

`offline_algorithm_smoke.py`遇到空`parking_points`时不再抛出`IndexError`，而是返回可解释的`SKIPPED_EMPTY_PARKING_POINTS`。

### 4.3 统一跨平台入口

新增`scripts/run_offline_checks.py`，统一：

- 选择可用Python；
- 验证sample、正式`_1`和正式`_2`；
- 逐项输出PASS/FAIL/SKIPPED；
- 保留stderr；
- 任一必需项失败时返回非零退出码；
- 兼容Windows和Ubuntu路径。

具体作用是把过去依赖人工参数和单脚本执行的检查，收敛成开发者和CI使用同一入口。

## 5. Template与仓库一致性治理

新增自动检查约束：

- `scripts`作为offline validator的canonical source；
- production YAML作为配置canonical source；
- sample AD Package通过SHA-256检查复制关系；
- expected tree检查Interfaces、Planning、Control、Simulation、Chassis和PlanMission相关文件是否缺失；
- UTF-8、JSON、Markdown本地链接和生成目录污染检查。

具体作用是防止模板、运行脚本和实际package长期手工维护后悄然漂移。

## 6. GitHub Actions CI

新增`.github/workflows/ros2_humble_ci.yml`，包括：

- Ubuntu 22.04与ROS2 Humble；
- rosdep安装；
- full colcon build/test/test-result；
- offline runner；
- clang-format检查；
- ASan/UBSan job；
- test-result和log artifact上传；
- commit、ROS distro、compiler信息输出；
- Chassis test mode与禁止真实硬件路径。

具体作用是让同一提交的production-linked C++、ROS2 integration和离线数据合同可以在统一环境中形成可追踪证据。

## 7. 测试规模与证据状态

Phase 14完成时建立了：

- 48个C++ `TEST`源码case；
- 8个ament/CTest target；
- 4个ROS2/ament Python method；
- 17项统一offline checks。

本地Windows实际结果：

- offline checks：17/17 PASS；
- C++与ROS2：未执行；
- CI在最初报告时为`CONFIGURED_NOT_EXECUTED`。

后续历史GitHub run `29152378189`中sanitizer job通过了当时的Planning/Control测试，主build-test因container内`git ls-files`返回128而失败，因此总体仍是`EXECUTED_FAIL`，不能写成full CI PASS。

## 8. 解决的问题与剩余作用

Phase 14修复或缓解：

- `CDX-P1-006`：从几乎没有生产C++测试，提升到Planning、Control、Chassis production-linked gtest底座；
- `CDX-P1-007`：建立CI配置和证据管线，但没有同提交full PASS，不能关闭；
- `CDX-P3-001`：修复失效默认路径；
- `CDX-P3-002`：修复空parking list未处理异常；
- `CDX-P3-003`：建立template/config/sample漂移检查。

这一阶段的核心价值是“让后续Phase 15和Phase 16的生产修改有地方写直接测试并进入CI”，而不是完成所有算法或安全功能本身。

