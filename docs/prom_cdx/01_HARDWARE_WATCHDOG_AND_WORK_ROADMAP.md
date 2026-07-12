# 硬件 Watchdog 边界与 Planning / Control 后续工作安排

## 1. 决策结论

项目采用以下最终失联停车策略：

```text
Control 正常运行
  -> 周期发布 SCU command
  -> Chassis Driver 按消息到达发送 0x121
  -> 底盘持续接收 0x121

Control / DDS / Driver / 主机链路异常
  -> 底盘连续 500 ms 收不到 0x121
  -> 底盘硬件 watchdog 独立触发停车
```

在该合同真实、不可绕过且经过供应商或台架验证的前提下，可以不在 `yunle_chassis` 中重复实现软件 watchdog。后续优化应集中在 Planning 和 Control，同时保持 Chassis 代码、CAN ID、DBC mapping、topic、service、配置和 transport 不变。

## 2. 必须写清的安全边界

硬件 watchdog 可覆盖：

- Control 进程崩溃后不再发布；
- DDS 链路中断导致 Driver 收不到命令；
- Driver 进程崩溃或主机断电导致 0x121 消失；
- 主机到 CAN 网关的通信中断导致底盘收不到 0x121。

硬件 watchdog 不能自动证明：

- 500 ms 内车辆的最大继续行驶距离符合安全要求；
- 底盘收到格式合法但语义错误的连续 0x121 时会停车；
- brake、steering、shift 的协议映射正确；
- 网关或 VCU 固件本身无故障；
- 机械制动、执行器和物理急停有效；
- 500 ms 阈值在所有车型、固件和运行模式中一致。

因此 Control 仍必须对 emergency、failure、invalid input、VehicleState、SafetyStatus 和输入 timeout 主动发布零速制动命令。硬件 watchdog 只处理“0x121 完全消失”，不替代 Control 的语义安全状态机。

## 3. 500 ms 合同的证据要求

在车辆或 bench 放行前，至少应保存：

1. 供应商协议或正式接口说明，明确 0x121 timeout 为 500 ms 或更短；
2. timeout 后的确定动作：制动、禁能、shift 和 steering 行为；
3. CAN 抓包：正常周期、停止发布时刻、最后一帧 0x121、硬件停车触发时刻；
4. 故障注入：停止 Control、杀死 Driver、断开主机网络、主机断电；
5. 不同车型/固件版本的适用性；
6. 最大停车延迟和停车距离；
7. 恢复条件：0x121 恢复后是否立即重新允许运动，是否需要人工确认；
8. 物理急停和硬件 watchdog 之间的优先级。

若这些证据尚未取得，应记录为 `DECLARED_NOT_HIL_VERIFIED`，不能写成 `HIL_PASS`。

## 4. 对现有 Finding 的处置

| Finding | 建议状态 | 说明 |
|---|---|---|
| `CDX-P0-001` | `FIXED` | Control 已消费 Planning emergency/failure 语义；仍需完整 ROS2 integration 证据。 |
| `CDX-P0-002` | `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION` | Driver 软件没有 watchdog，但系统选择由底盘 500 ms 硬件 watchdog 承担最终失联停车。只有硬件证据齐全时才可写 `MITIGATION_VERIFIED`。 |
| `CDX-P1-004` | `OPEN` | Control 参数、limiter/smoother 和实际 dt 仍需完善。 |
| `CDX-P1-005` | `OPEN` | Planning manifest path containment 仍需实现。 |
| `CDX-P1-006` | `PARTIALLY_FIXED` | C++ tests 已建立并有 sanitizer 执行证据，但覆盖矩阵仍不完整。 |
| `CDX-P1-007` | `PARTIALLY_FIXED / CI_EXECUTED_FAIL` | sanitizer job PASS，但主 build-test 因 repository hygiene 的 Git 子进程失败。 |

## 5. 后续两阶段安排

### Phase 15：Planning 完整性与正确性

目标：让 Planning 对恶意/损坏 AD Package fail closed，保证 Dijkstra/A* 可重复，拆出并测试 semantic/trajectory helper，补齐 Planning service 和状态集成测试。

主要工作：

- manifest path、absolute path、`../`、symlink escape containment；
- duplicate ID、负/非有限 cost、index/count/edge/waypoint 一致性；
- A* admissible/weighted 语义和 deterministic tie-break；
- semantic anchor、linked node/edge、null、same-edge、reverse-disabled；
- route length/time、full/local trajectory 终点和连续性；
- production-linked gtest 和 Planning ROS2 integration；
- 修复当前 CI hygiene 阻塞并保存同提交证据。

禁止：修改 Control 生产算法、`src/yunle_chassis`、正式 Roadnet 数据、Nav2/Lanelet2 替换和高级 Planning 算法。

### Phase 16：Control 工程化与硬件 Watchdog 合同

目标：在不修改 Chassis 的前提下，提高 Control 参数安全、控制输出稳定性、可诊断性和输出周期保证，并验证 0x121 消失后由 500 ms 硬件 watchdog 停车的系统合同。

主要工作：

- 参数 schema/fail-fast，清理 silent no-op keys；
- limiter/smoother 使用真实 dt，独立前后轮 rate、accel/decel/jerk；
- controller NaN/Inf、零速、终点、倒车和 progress window；
- algorithm switch/reset 状态；
- emergency stop 始终旁路 normal smoothing；
- Control/SCU 输出频率、jitter、deadline 和 dropout 诊断；
- localization/trajectory/VehicleState/estop clear 的 ROS2 integration；
- bench/HIL 验证停止 0x121 后 500 ms 内硬件停车。

禁止：修改 `src/yunle_chassis`、DBC/CAN/topic/service、实现高级控制器、修改正式 Roadnet。

## 6. 优先级表

| Priority | Work item | Reason | Dependencies | Verification | Completion criteria |
|---|---|---|---|---|---|
| P0 contract | 固化 500 ms 硬件 watchdog 证据 | 它替代 Driver 软件 watchdog | 供应商/台架/车辆 | CAN capture、故障注入 | 合同、触发动作、恢复条件和适用版本明确 |
| P1 | Phase 15 Planning hardening | 数据和规划错误会产生错误轨迹 | Phase 14 production test target | gtest、launch、CI | loader/planner/semantic matrix 通过 |
| P1 | 修复 CI 主 job | 当前总体 CI 为失败 | Git safe-directory/hygiene | GitHub Actions | full build/test/result 和 artifacts PASS |
| P1 | Phase 16 Control engineering | 保证语义 stop、周期和控制稳定性 | Phase 15 接口保持兼容 | gtest、launch、cadence、HIL | 参数/输出/timeout/500 ms 合同通过 |
| P2 | 文档与配置治理 | 防止 no-op 参数和合同漂移 | 两阶段完成 | config consistency | canonical 参数和验证文档一致 |
| P3 | 闭环仿真与高级算法 | 基础正确性完成后扩展 | P0/P1 完成 | SIL/HIL | 独立阶段验收 |

## 7. 两阶段共同约束

- 不修改 `src/yunle_chassis/**`。
- 不把硬件 watchdog 写成 Chassis 软件能力。
- 不改变 canonical AD Package 合同。
- 不修改正式 Roadnet 数据来通过测试。
- production C++ test 必须直接链接 production target。
- ROS2 不可用时诚实标记 `SKIPPED_ROS2_UNAVAILABLE`。
- CI workflow 必须以同一 commit 的实际结果为准。
- 每阶段单独提交、单独报告、完成后停止。

