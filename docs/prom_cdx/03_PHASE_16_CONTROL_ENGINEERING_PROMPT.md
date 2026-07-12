# Phase 16 Control 工程化与硬件 Watchdog 合同 Prompt

以下内容可在 Phase 15 完成并复核后直接复制执行：

````markdown
你现在位于 ROS2 低速自动驾驶项目根目录。

本任务是 Phase 16：在完全不修改 `src/yunle_chassis` 的前提下，提高 Control 参数安全、控制输出稳定性、控制器鲁棒性、诊断和 ROS2 回归，并验证与“底盘连续 500 ms 未收到 CAN 0x121 即硬件停车”合同的兼容性。

不要实现 Chassis 软件 watchdog。硬件 watchdog 是项目明确采用的最终失联停车机制，但未经供应商/台架证据验证时不得写成 HIL PASS。

## 一、必须先阅读

完整阅读：

- 根目录 `AGENTS.md`
- `docs/prom_cdx/00_INDEX.md`
- `docs/prom_cdx/01_HARDWARE_WATCHDOG_AND_WORK_ROADMAP.md`
- `docs/prom_cdx/02_PHASE_15_PLANNING_INTEGRITY_PROMPT.md`
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/phase_15_report.md`
- `reports/final_generation_report.md`
- `docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `docs/audit_cdx/05_OPTIMIZATION_ROADMAP.md`
- `docs/03_ros2_interfaces.md`
- `docs/05_control_module_design.md`
- `docs/07_config_launch_runtime.md`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/OPERATOR_STARTUP_CHECKLIST.md`
- `docs/LQR_CONTROLLER_DESIGN.md`
- `src/low_speed_av_control/CMakeLists.txt`
- `src/low_speed_av_control/package.xml`
- `src/low_speed_av_control/include/low_speed_av_control/**`
- `src/low_speed_av_control/src/**`
- `src/low_speed_av_control/test/**`
- `src/low_speed_av_control/config/control_params.yaml`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `src/low_speed_av_bringup/test/**`
- `.github/workflows/ros2_humble_ci.yml`
- `scripts/run_offline_checks.py`

文件缺失时记录，不得假定存在。

## 二、当前已知状态

按当前代码重新核验：

- Control 已保存 trajectory metadata，并在 controller 前拒绝 emergency/failure/invalid trajectory。
- Control 已有 `WAIT_INPUTS`、`READY`、`ACTIVE`、`CONTROLLED_STOP`、`ESTOP_LATCHED` 状态机。
- VehicleState autonomous/brake/fault、输入 timeout和显式 clear已形成门控。
- 默认 `output.mode=both`，发布 internal command、status和SCU command。
- `test_safety_state_machine`、`test_controllers`、`test_vehicle_command_pipeline` 直接链接 production library；Phase 14 sanitizer中18个Control C++ cases实际PASS。
- `CDX-P1-004` 参数/limiter/smoother仍不完整：部分YAML key未读取，前后轮rate、accel/decel/jerk与真实dt未形成完整合同。
- controller NaN/Inf、reverse、progress window、algorithm switch/reset和Node-level integration覆盖不足。
- 底盘硬件合同：连续500 ms无0x121触发停车；Chassis Driver仍按每条ROS SCU消息发送一次0x121。
- 因此Control正常ACTIVE或主动stop期间必须保持稳定发布周期；Control/Driver/主机完全失效时由硬件500 ms timeout停车。
- 严禁修改 `src/yunle_chassis`。

## 三、目标

1. 所有安全关键Control参数启动时fail-fast；
2. YAML不存在silent no-op安全参数；
3. limiter/smoother使用真实dt并正确限制速度、accel、decel、jerk和前后转角rate；
4. emergency/hard stop始终绕过normal smoothing；
5. 四种controller和两种vehicle model对合法/非法/终点/零速输入行为确定；
6. tracking progress在回环轨迹不全局跳变；
7. algorithm/trajectory切换时状态明确reset；
8. `/control/command`、`/control/status`、SCU command保持稳定周期，正常jitter与deadline相对500 ms硬件阈值有充分裕度；
9. Control ROS2 integration matrix实际可执行；
10. 形成500 ms硬件watchdog合同、bench/HIL步骤和证据状态；
11. 不修改Chassis代码、协议或配置。

## 四、允许修改范围

- `src/low_speed_av_control/**`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `src/low_speed_av_bringup/test/**`
- 必要的Control test helper、test-only launch/config
- Control/Bringup CMake和package.xml
- Control相关README/docs、接口文档（仅真实合同变化）
- offline runner/template config consistency（仅同步Control配置和测试）
- `.github/workflows/ros2_humble_ci.yml`（仅Control测试/证据和既有CI问题）
- audit finding文档
- `reports/phase_15_report.md`
- `reports/final_generation_report.md`
- 新增 `reports/phase_16_report.md`

## 五、禁止修改范围

- 严禁修改 `src/yunle_chassis/**`。
- 不改变0x121 CAN ID、DBC mapping、SCU消息字段、topic、service和网关配置。
- 不修改Planning生产算法或AD Package合同；仅允许test integration使用其现有输出。
- 不修改正式Roadnet数据。
- 不新增未文档化自定义接口。
- 不实现完整MPC、reverse高级控制、闭环仿真或障碍物系统。
- 不把500 ms硬件watchdog写成软件功能。
- 不以Python replica、token check或未执行测试证明C++行为。

## 六、开始前基线

执行并记录：

- `git status --short`
- `git diff --stat`
- `git diff --check`
- 当前SHA、branch、remote tracking
- ROS2/colcon/cmake/compiler/pytest/uv/Python
- build/install/log/cache状态
- Phase 15实际结果和CI状态
- Control production target/test target关系
- YAML所有leaf key与Node declare/get映射
- controller、vehicle model、limiter、smoother、state machine、mapper当前行为
- Control timer、publish rate、status rate和SCU topic路径
- 现有C++/launch test case数和实际执行结果
- 500 ms硬件watchdog供应商/bench证据是否存在

修改前运行最大可行offline和ROS2测试。

## 七、硬件 Watchdog 协同合同

明确区分：

### Control主动安全停车

当Control仍存活但发生trajectory/localization/VehicleState/SafetyStatus异常时：

- 继续按control timer周期发布stop；
- internal speed=0、enable=false、brake安全值、reason稳定；
- SCU target speed=0、brake enable=true、转角使用明确安全策略；
- 不等待500 ms硬件timeout。

### 硬件失联停车

当Control、DDS、Driver、主机或网络导致0x121完全消失时：

- 底盘硬件在500 ms内触发停车；
- 软件不得声称决定硬件具体制动力和停车距离；
- 恢复条件必须来自供应商合同。

### 发布周期裕度

- 默认50 Hz对应20 ms周期；不得把500 ms作为正常允许jitter。
- 定义Control输出deadline和告警阈值，建议正常最大间隔远小于500 ms，并在配置/测试中明确。
- 记录publish interval max/p95、missed cycles和last publish age。
- Control仍存活时不得故意停止发布来触发硬件watchdog，除非供应商合同明确要求且经过专项评审。

## 八、参数和配置治理

### 8.1 Fail-fast

至少校验：

- rate、timeout、dt有限且>0；
- speed/accel/decel/jerk/steer/rate limit有限且>=0；
- wheelbase>0；rear ratio合法；
- LQR/MPC权重和迭代参数合法；
- output.mode、controller、vehicle model、status allowlist合法；
- SCU limit、sign、overrange policy、stop shift合法；
- clear speed threshold合法。

非法安全参数统一启动失败；若确需安全默认回退，必须限定字段并记录FATAL/ERROR，不得静默。

### 8.2 No-op key

- 枚举production和bringup YAML leaf keys。
- 对比Node declare/get和production option使用。
- 每个key必须：真实实现、删除，或明确标记deprecated并启动告警。
- template config同步，差异进入结构化allowlist并写原因。

## 九、Limiter / Smoother

- 使用实际steady-clock/control-period dt，并对异常dt安全处理。
- 分开max accel、max decel和jerk；不得只用固定speed step冒充。
- 前后轮转角rate独立限制。
- 多周期state确定；reset API明确。
- 新trajectory ID、controller switch、vehicle model switch、重新使能时明确reset/hold策略。
- emergency_stop、brake、enable=false和state machine stop立即旁路normal smoothing。
- stop输出固定零速度、零或明确安全转角、brake、disable和非空reason。

不要借此重写controller算法。

## 十、Controller / Vehicle Model 鲁棒性

四controller至少覆盖：

- nominal有限确定输出；
- empty/single/final trajectory；
- pose/state/trajectory NaN/Inf fail closed；
- zero/low speed；
- forward/reverse gear合同；
- trajectory ID切换；
- 回环路线progress window；
- 相同输入输出确定；
- unsupported reverse场景明确stop，不假装支持。

两vehicle model覆盖公式、limit和非法wheelbase/ratio的fail-fast。

## 十一、Progress 和切换状态

- 引入ROS-independent tracking progress helper或等价production逻辑。
- 最近点搜索限制在可配置窗口，结合heading、gear和trajectory identity。
- progress正常单调，允许显式reset。
- 新trajectory、reload、algorithm switch、estop clear后的状态迁移明确。
- service切换controller时不得保留不兼容smoother/controller state。

## 十二、状态和诊断

保持 `/control/status` 合同，至少可诊断：

- safety state和stop reason；
- input freshness/age；
- active controller/model；
- last command publish age；
- publish interval max/p95或bounded counters；
- missed control cycles；
- saturation/limiter状态；
- hardware watchdog contract status：`DECLARED_NOT_HIL_VERIFIED`或`HIL_VERIFIED`，不得伪造。

如现有ModuleStatus字段不足，优先日志/标准diagnostic_msgs；新增自定义字段前必须更新接口合同并单独论证。

## 十三、Production-linked C++ 测试

继续使用ament_cmake_gtest并直接链接`low_speed_av_control`。

至少覆盖：

- 四controller正负矩阵；
- 两vehicle model；
- limiter speed/accel/decel/jerk；
- front/rear steer和rate；
- real/fake dt边界；
- smoother多周期和reset；
- emergency bypass；
- invalid参数fail-fast helper；
- SafetyStateMachine完整优先级；
- ScuCommandMapper单位、gear、sign、clamp/zero/NaN/stop；
- progress window和trajectory switch；
- publish cadence decision/counter的pure逻辑。

时间测试使用fake clock，不依赖长sleep。

## 十四、ROS2 Integration

使用bounded launch_testing，且不启动Chassis Driver。至少覆盖：

1. Planning failure/emergency -> internal和SCU brake stop；
2. 四controller切换时emergency语义不变；
3. localization timeout；
4. trajectory timeout；
5. VehicleState autonomous disabled/brake/fault/timeout；
6. estop latch，普通OK不clear；
7. clear service拒绝/成功，成功后先READY；
8. output.mode=both默认合同；
9. Control/SCU topic正常发布频率和最大间隔；
10. late subscriber/QoS；
11. process shutdown和失败诊断。

硬件500 ms停车属于bench/HIL，不在纯launch test中伪造PASS。

## 十五、Bench / HIL 500 ms 验证

只编写和执行安全流程，不修改Chassis代码：

- wheels-off或安全台架；
- 记录0x121 CAN时间戳；
- 正常Control运行确认周期和jitter；
- 停止Control publisher/杀死Control；
- 杀死Driver；
- 断开主机网络；
- 测量最后0x121到硬件停车触发时间；
- 确认<=500 ms；
- 记录制动、shift、steering、恢复行为和固件版本。

无法执行时标记`HIL_NOT_EXECUTED`，不得写PASS。

## 十六、CI 和执行规则

ROS2可用时先定向后全量：

```bash
colcon build --symlink-install --packages-up-to low_speed_av_control low_speed_av_bringup
colcon test --packages-select low_speed_av_control low_speed_av_bringup --event-handlers console_direct+
colcon test-result --verbose
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
python3 scripts/run_offline_checks.py
```

ROS2不可用时运行offline、config/template、repository hygiene、JSON/UTF-8/Markdown link、CMake/package static consistency、clang-format和`git diff --check`；C++/launch准确标记未执行。

CI必须执行full build/test/result、Control launch、sanitizer并上传artifacts。状态只能是`EXECUTED_PASS`、`EXECUTED_FAIL`、`CONFIGURED_NOT_EXECUTED`或`NOT_CONFIGURED`。

## 十七、文档和报告

更新Control README、control design、config/runtime、SCU output、operator checklist、testing matrix、audit findings、final report，并新增`reports/phase_16_report.md`。

报告必须记录硬件watchdog证据来源和状态，不得将项目方声明写成实测结果。

## 十八、Finding 状态规则

- `CDX-P0-001` 仅在生产和integration证据未回归时保持FIXED；破坏则重新打开。
- `CDX-P0-002` 使用`OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`；只有硬件证据齐全时追加`MITIGATION_VERIFIED`，不得写软件FIXED。
- `CDX-P1-004` 只有参数、limiter/smoother合同和测试完整才可FIXED。
- `CDX-P1-006` 依据实际C++覆盖和执行结果。
- `CDX-P1-007` 只有同提交full CI PASS才可FIXED。
- 其它Control findings逐项依据代码和测试判断。

## 十九、完成标准

- safety关键参数fail-fast；
- no-op keys被实现/删除/deprecate；
- limiter/smoother真实dt和完整rate/accel/decel/jerk；
- emergency立即旁路；
- controller/model非法输入fail closed；
- progress/switch/reset确定；
- Control和SCU输出周期远小于500 ms并有诊断；
- integration matrix完整且实际结果准确；
- 硬件watchdog合同有文档，HIL状态不伪造；
- offline/CI最大可行验证完成；
- `git diff --check` PASS；
- `git diff -- src/yunle_chassis` 为空；
- 未修改正式Roadnet；
- 未进入高级算法/闭环仿真。

## 二十、最终回复

只汇报实际Control生产修改、参数和周期合同、test targets、实际PASS/FAIL/SKIPPED、CI/HIL状态、finding状态、500 ms硬件边界和剩余风险。完成后停止，不自动实施闭环仿真或高级算法。
````

