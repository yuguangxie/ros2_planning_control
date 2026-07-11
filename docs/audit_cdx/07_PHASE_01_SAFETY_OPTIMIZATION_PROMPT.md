# 第一阶段完整 Prompt：端到端安全闭环优化

下面的 Prompt 可直接复制到一个新的 Codex 任务中执行。本阶段优先关闭审计中的 CDX-P0-001、CDX-P0-002、CDX-P1-001、CDX-P1-002 和 CDX-P1-003；不要在本阶段重写高级规划算法。

```text
你现在位于 ROS2 低速自动驾驶项目根目录。请先完整阅读根目录 AGENTS.md，以及以下审计文档：

docs/audit_cdx/00_AUDIT_INDEX.md
docs/audit_cdx/01_EXECUTIVE_SUMMARY.md
docs/audit_cdx/02_ARCHITECTURE_AND_MODULE_AUDIT.md
docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md
docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md
docs/audit_cdx/05_OPTIMIZATION_ROADMAP.md

本任务是审计后的第一阶段优化：修复 Planning -> Control -> Yunle SCU -> Chassis Driver 的端到端安全语义，关闭以下问题：

- CDX-P0-001：Planning failure/emergency stop 在 Control 入口被丢弃。
- CDX-P0-002：Chassis Driver 没有独立命令超时看门狗。
- CDX-P1-001：VehicleState 自治许可、人工制动和故障字段没有形成控制门控。
- CDX-P1-002：latched estop 可被普通 OK 心跳自动清除。
- CDX-P1-003：默认不发布规范要求的 /control/command。

目标不是只修改文档或增加 token 检查，而是修改生产 C++、接口依赖、配置、launch、离线测试和说明文档，使安全行为在所有受支持控制器与车辆模型下保持一致。

## 一、不可违反的约束

1. 保持 Planning、Control、Interfaces、Bringup、Simulation、Chassis 职责分离。
2. 不使用 Nav2/Lanelet2 取代自定义 Roadnet 架构。
3. 不修改 canonical AD Package 合同，不引入旧路径 manifest.json、trajectory/waypoints.json 或根目录 validation_report.json。
4. 不随意修改现有 msg/srv 字段。新增自定义接口前必须先核对 docs/03_ros2_interfaces.md；本阶段优先使用现有接口和标准 ROS2 service/message。
5. 默认速度继续保持低速保守值。
6. 安全停车必须优先于 limiter、smoother、controller 和 vehicle model 的正常输出。
7. 不依赖某一个控制器的偶然行为实现安全。Pure Pursuit、Stanley、LQR、MPC sampler 必须具有一致的 stop 结果。
8. 当前 Codex 环境可能没有 ROS2。没有 ROS2 时不得把 colcon/ros2 不可用写成代码失败，也不得声称已构建成功。
9. 只修改与本阶段目标相关的文件；保留用户已有改动，不重置工作区。
10. 所有 Markdown 使用 UTF-8 和清晰中文；代码/配置 identifier 使用英文。

## 二、开始前检查

执行并记录：

- git status --short
- rg --files
- 检查当前是否存在 ros2、colcon、C++ 编译器和可用 Python。
- 阅读 ControlNode、ScuCommandMapper、CommandLimiter、CommandSmoother、VehicleState、Trajectory、Planning failure trajectory、ControlCommandBridge、ChassisDriverNode、UDP channel、相关 CMake/package/config/launch。
- 运行当前可运行的离线 smoke，保存修改前基线；脚本默认路径失效时可显式传入正确 package，但要记录该问题，不得伪造 PASS。

## 三、Control：完整消费 Trajectory 安全元数据

修改 low_speed_av_control，使 ControlNode 不再只复制 trajectory points。

必须保存和检查至少以下消息级信息：

- trajectory_id
- source_package_id
- status
- emergency_stop
- receive time
- points validity

默认只允许明确的正常 trajectory status 进入跟踪，例如 Planning 当前发布的 ok。允许列表应配置化或集中定义，不能把任意未知字符串默认为安全。

以下任一条件必须立即进入 stop，并禁止调用正常 controller 产生运动命令：

- msg.emergency_stop == true
- status 表示 failure/emergency/invalid
- points 为空
- 任一点 x/y/yaw/kappa/s/v 非有限值
- s_m 明显非单调且不能安全解释
- gear 不在允许集合
- trajectory 已超时

Planning failure trajectory 即使包含一个点、包含非零速度点，或当前 controller 为任意四种算法，都必须输出一致的 controlled/brake stop。

新增状态字段时保持 ROS-independent logic 尽可能独立，方便下一阶段直接写 C++ 单测。

## 四、Control：建立明确的安全状态机

不要继续用分散 if 语句隐式表达所有状态。至少建立以下可诊断状态：

WAIT_INPUTS
READY
ACTIVE
CONTROLLED_STOP
ESTOP_LATCHED

明确每个状态的进入条件、退出条件和 stop reason 优先级。建议优先级从高到低：

1. safety estop / explicit emergency trajectory
2. vehicle fault / autonomous disabled / brake pressed
3. localization timeout / invalid localization
4. trajectory timeout / invalid trajectory / planning failure
5. normal tracking

VehicleState 处理要求：

- 将 autonomous_enabled、brake_pressed、fault_code 保存到内部状态。
- 支持配置 vehicle_state.required，兼容当前“vehicle state if available”的架构。
- 当 required=true 时，未收到或超时必须停车。
- 一旦收到 VehicleState，即使 required=false，autonomous_enabled=false、brake_pressed=true 或非空 fault_code 也不得继续输出运动命令。
- 为 VehicleState 增加可配置 timeout。
- 对所有输入做 NaN/Inf 检查。

对输入 header stamp 的策略必须写清：至少使用 receive time 保证本地 watchdog；可选检查 message stamp，但不能因仿真时间/零时间戳误判。将策略写入配置与文档。

## 五、显式 estop 清除机制

默认 safety.estop_latched=true 时，普通 ok/standby heartbeat 不得自动清除锁存急停。

优先实现标准 ROS2 Trigger service，例如：

/low_speed_av_control/clear_estop
std_srvs/srv/Trigger

不得为此随意发明未文档化的自定义字段。

clear_estop 成功前必须至少满足：

- 最近 safety 状态不再请求 estop/failure；
- 车辆速度绝对值低于可配置阈值；
- 没有 vehicle fault；
- brake/autonomous 条件满足项目定义；
- localization/trajectory 等恢复条件明确。

若条件不满足，service 返回 success=false 和可操作的原因。清除操作不得立即产生跳变运动命令；必须回到 READY，等待下一控制周期重新验证所有输入。

如果当前环境或架构使 service 方案不可行，必须说明理由并实现同等强度的“仅显式 clear”机制，不能退回普通 ok 自动清除。

## 六、Control 输出合同

将生产默认配置调整为同时发布：

- /control/command
- /control/status
- /yunle_chassis/control/scu_control_command

建议默认 output.mode=both，并同步：

- src/low_speed_av_control/config/control_params.yaml
- src/low_speed_av_bringup/config/control_params.yaml
- 相关 README、topic 文档、操作文档

若选择其他设计，必须仍满足 AGENTS.md 中“发布 /control/command 和 /control/status”的要求。

所有 stop 输出必须满足：

- speed_mps == 0
- enable == false
- emergency_stop 根据 stop 等级正确设置
- brake 为安全停车值
- reason 非空且稳定
- SCU target speed == 0
- SCU brake enable == true
- 前后轮转角采用明确的安全策略，不保留未经论证的旧转角

不要让 normal smoother 延迟 emergency stop。controlled stop 与 hard estop 的减速度/制动语义要在文档中区分；若 SCU 接口只有 bool brake，必须明确当前映射限制。

## 七、Chassis Driver：独立命令调度与 watchdog

为 SCU_Control_Command 建立 driver 内部独立安全层。不能继续只在 ROS subscription callback 中发送一次运动 CAN frame。

最低要求：

1. 订阅回调只负责验证并缓存最新有效 SCU command 与 receive time。
2. 使用 driver 内部周期 scheduler 以可配置频率发送 0x121，推荐默认 20–50 Hz。
3. 节点启动后、收到第一条有效命令前，周期发送安全 stop frame。
4. 最新命令超过 command_timeout_s 后，周期发送 stop frame，不再重放旧运动命令。
5. 无效 shift、NaN/Inf、越界值必须 fail closed；不能因丢弃无效消息而继续无限重放更早的运动命令。
6. 节点正常 shutdown/destructor 前 best-effort 发送 stop frame；文档明确进程硬崩溃仍需底盘硬件 watchdog。
7. 线程/定时器访问缓存和 UDP TX 必须正确同步，不引入 data race 或死锁。
8. 其它非周期 debug/torque/chassis command 保持原有行为，除非有明确安全理由调整。

建议参数：

scu_control_publish_rate_hz
scu_control_command_timeout_s
scu_control_startup_stop_enabled
scu_control_shutdown_stop_enabled
scu_control_stop_shift_level

参数名可按现有命名规范调整，但 Control 与 Chassis 的 stop shift、speed/steering limit 和单位必须一致。

增加标准 diagnostics 输出，优先使用 diagnostic_msgs/msg/DiagnosticArray，至少报告：

- scheduler state
- last command age
- watchdog active
- last stop reason
- TX success/failure count
- CAN1/CAN2 channel state

不要为了 diagnostics 让 chassis_driver 反向依赖 low_speed_av_control。

## 八、安全关键参数校验

本阶段至少对新增参数和直接参与 stop/watchdog 的现有参数做 fail-fast 校验：

- rate > 0 且有限
- timeout > 0 且有限
- speed/steer/decel limit >= 0 且有限
- stop shift 合法
- output.mode 属于允许集合
- safety clear speed threshold >= 0

非法安全参数不得静默进入 std::clamp 或产生未定义行为。选择“启动失败”或“回退安全默认值”时要统一，并在 status/log 中清晰说明。

完整的 accel/decel/jerk smoother 重构不属于本阶段强制范围，除非为实现安全 stop 必须修改。不要把阶段扩展成控制算法重写。

## 九、离线测试与可验证逻辑

即使没有 ROS2，也必须新增或扩展可运行的离线检查，覆盖真实安全合同，而不是只搜索 token。

最低覆盖矩阵：

- emergency_stop=true + 单个零速点，4 controllers x 2 vehicle models -> brake stop
- emergency_stop=true + 非零速度点 -> brake stop
- status=failure 且 points 非空 -> brake stop
- invalid/NaN trajectory -> brake stop
- trajectory timeout -> brake stop
- localization timeout -> brake stop
- autonomous_enabled=false -> brake stop
- brake_pressed=true -> brake stop
- fault_code 非空 -> brake stop
- safety estop 后普通 ok 不清除
- 显式 clear 条件不满足 -> 拒绝
- 显式 clear 条件满足 -> 回 READY，不直接跳到运动
- chassis startup -> stop frame
- chassis fresh command -> 周期运动 frame
- chassis command timeout -> 周期 stop frame
- 无效新命令到达 -> 不继续重放旧运动命令

优先将状态判断、watchdog 决策和 frame 构造提取为纯 C++ 可测试逻辑；若当前无编译器，仍要生成正确的 C++ 测试源并补 Python offline contract test，在报告中区分“已运行 Python”与“待 ROS2/C++ 执行”。

不得只写 assert 某个源码字符串存在来宣称安全功能通过。

## 十、配置、launch 和文档同步

更新所有重复配置，不允许只改一个副本：

- src/low_speed_av_control/config/control_params.yaml
- src/low_speed_av_bringup/config/control_params.yaml
- src/yunle_chassis/chassis_driver/config/chassis_driver.yaml
- 必要的 launch 文件

更新至少以下文档：

- README.md
- src/low_speed_av_control/README.md
- src/yunle_chassis/README.md
- docs/03_ros2_interfaces.md（仅当接口合同确实变化）
- docs/05_control_module_design.md
- docs/07_config_launch_runtime.md
- docs/YUNLE_SCU_COMMAND_OUTPUT.md
- docs/OPERATOR_STARTUP_CHECKLIST.md

文档必须说明：

- failure trajectory 的端到端 stop 语义；
- stop reason 优先级；
- estop clear 操作与前置条件；
- Control watchdog 与 Chassis watchdog 的不同职责；
- driver process 硬崩溃仍依赖硬件 watchdog；
- sim、bench、vehicle 环境的安全边界。

## 十一、构建与检查

先检测环境：

- 如果 ROS2/colcon 可用，运行定向 build/test，再运行全量 build/test：
  - colcon build --symlink-install --packages-up-to low_speed_av_control chassis_driver low_speed_av_bringup
  - colcon test --packages-select low_speed_av_control chassis_driver
  - colcon test-result --verbose
- 如果 ROS2 不可用，不执行或伪造上述命令，在报告中逐条写 SKIPPED_ROS2_UNAVAILABLE。
- 运行全部当前可用 Python validator/smoke，包括 sample 与两套正式 Roadnet 包相关检查。
- 运行 git diff --check。
- 检查配置 key、topic、CMake dependency、package.xml dependency 一致性。

## 十二、禁止事项

- 不实现 Frenet/Hybrid A*/完整 MPC 等无关高级算法。
- 不修改正式 Roadnet 数据内容来让测试通过。
- 不删除历史报告或用户文件。
- 不把 LQR 的零速特殊分支当成 emergency flag 修复。
- 不只在 ScuCommandMapper 做补丁而保留 Control 安全状态丢失。
- 不让 Chassis watchdog 依赖 Control 持续发布 stop。
- 不声称软件 watchdog 能覆盖 driver 进程硬崩溃或断电。

## 十三、阶段交付物

必须交付：

1. 完整生产代码修改。
2. CMakeLists.txt/package.xml 依赖更新。
3. config/launch 同步修改。
4. 安全合同离线测试与 C++ 测试源。
5. 相关中文文档更新。
6. reports/phase_13_report.md。

phase_13_report.md 必须包含：

# Phase 13 Report
- Goal
- Files changed
- Key design decisions
- AD Package compatibility notes
- Config/topic compatibility notes
- Tests or offline checks run
- ROS2 commands skipped because ROS2 is unavailable
- Known limitations
- Next phase handoff

报告还要逐项列出 CDX-P0-001、CDX-P0-002、CDX-P1-001、CDX-P1-002、CDX-P1-003 的状态：FIXED、PARTIALLY_FIXED 或 OPEN，并提供文件/行号与测试证据。

## 十四、最终验收标准

只有同时满足以下条件，阶段一才能标为完成：

- Control 明确消费 Trajectory.emergency_stop 和 status。
- 四控制器、两车型对 emergency/failure trajectory 都输出同样的 brake stop。
- autonomous disabled、brake pressed、vehicle fault、input timeout 都不能输出运动命令。
- 普通 safety ok 不再自动清除 latched estop。
- 存在显式 clear 操作且有安全前置条件。
- /control/command 和 /control/status 默认发布。
- chassis_driver 周期发送 SCU command，并在 startup/timeout/shutdown 采用安全 stop。
- watchdog 状态可诊断。
- 可运行的离线检查通过；不可运行的 ROS2/C++ 命令被诚实标记为 SKIPPED。
- 没有修改 canonical AD Package 数据和 Planning/Control 分层。

完成后停止，不继续实施第二阶段测试体系或第三阶段 loader hardening；在最终回复中给出阶段结果、关键风险、运行命令和下一阶段明确交接。
```
