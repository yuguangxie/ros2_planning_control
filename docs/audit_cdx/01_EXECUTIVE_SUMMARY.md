# 执行摘要

## 总体判断

项目的技术方向正确，核心分层也基本成立：自定义 Low Speed Roadnet AD Package 进入 Planning，Planning 生成拓扑路线和连续轨迹，Control 完成轨迹跟踪和 Ackermann 转换，最后映射到 Yunle SCU 命令。项目没有把 Nav2 或 Lanelet2 当成主架构，符合仓库约束。

当前代码不是空骨架。RoadnetLoader、SHA-256、Dijkstra、A*、参考线拼接、曲率限速、语义目标、Pure Pursuit、Stanley、离散 LQR、Ackermann 模型、SCU 映射、RViz 可视化和仿真定位均有实际逻辑。两套正式 Roadnet 包和 canonical sample 也可通过现有 Python 校验。

但“代码量完整”尚未转化为“可安全交付”：控制层没有消费 `Trajectory.emergency_stop`，底盘驱动没有独立的命令超时制动，车辆自治状态和故障字段没有参与控制门控，默认配置还关闭了 `/control/command` 发布。现有自动检查绝大多数是在 Python 中重新实现或搜索 C++ 源码 token，没有直接验证 C++ 类和 ROS2 节点。

需要特别说明：Control 构造函数默认控制器是 Pure Pursuit，而随包 YAML 默认切换为 LQR。LQR 恰好会把“所有点均为零速”的轨迹转为 emergency stop，但它仍然没有读取消息级 emergency flag；只要 emergency trajectory 含非零速度点，或切换为其他受支持控制器，安全语义仍会丢失。因此该问题不能依赖当前 YAML 的 LQR 默认值规避。

## 成熟度评估

| 维度 | 评价 | 说明 |
|---|---:|---|
| 架构与职责分离 | 7.5/10 | Planning、Control、Interfaces、Bringup 边界清楚；Simulation、Chassis 扩展也基本独立。 |
| AD Package 合同 | 7/10 | canonical 路径、1.1.x、validation、checksum 和 waypoint index 已实现；根目录逃逸、重复 ID 和索引一致性仍不足。 |
| 规划功能 | 7/10 | 主链路可用，语义目标和轨迹连续性有较多实现；高级 motion/obstacle 算法仍是 fallback/stub。 |
| 控制功能 | 6/10 | 四类控制器、两类车型和 SCU 映射已接入；安全语义、状态门控、限速/限加速度实现不完整。 |
| 仿真与可视化 | 6.5/10 | 能形成位置回放闭环；它是路径回放器，不是车辆动力学或控制闭环仿真。 |
| 底盘驱动 | 5/10 | UDP-CAN 编解码和 DBC 映射可用；缺少 watchdog、源校验、通信健康状态和安全状态机。 |
| 自动化测试 | 3/10 | 离线 smoke 丰富，但只有一个 pytest 包装，且不直接执行 C++；Control/Chassis 无注册测试。 |
| 配置与文档一致性 | 5/10 | 文档覆盖广，但存在未声明/未使用参数、旧 launch 参数和大量历史结论并存。 |
| 发布与工程治理 | 3.5/10 | 无 CI、无仓库 LICENSE、无 sanitizer/静态分析、无 release gate；版本和 maintainer 仍是样例值。 |

## 可用性分级

| 场景 | 当前建议 |
|---|---|
| 离线数据检查与算法演示 | 可用，但应修复两个脚本的失效默认路径。 |
| RViz/仿真定位演示 | 基本可用；需要在 ROS2 环境对当前快照重新构建和回归。 |
| 消息监控、bench-only、wheels-off | 有条件可用；先修复 P0，并增加底盘 watchdog 与急停集成测试。 |
| 真实车辆低速运动测试 | 不建议。P0/P1 安全闭环和当前快照 ROS2 证据不足。 |
| 生产部署 | 不具备条件。缺少故障诊断、生命周期、测试覆盖、CI、发布和现场安全论证。 |

## 最主要优点

- canonical AD Package 合同已经落到 C++ loader，而不只是写在文档中。
- Planning/Control 的算法逻辑大多独立于 ROS 消息，具备进一步补 C++ 单测的基础。
- 默认速度保守，checksum、validation、no-go、speed-zone、超时停车等安全意识已进入设计。
- 规划语义目标、当前定位起点、全量参考路径与局部轨迹分离，说明项目已处理真实集成问题，而非最小 demo。
- Yunle SCU 单位转换、档位和转角限值有明确映射层，没有把底盘协议散落到控制算法中。

## 进入下一阶段前必须完成

1. 修复 `Trajectory.emergency_stop/status` 到 Control/SCU brake 的端到端传播。
2. 为 chassis driver 增加独立于 Control 的命令超时 watchdog 和周期安全帧。
3. 把 `autonomous_enabled`、制动、fault、安全状态纳入明确的控制许可状态机。
4. 增加直接链接生产 C++ 的 gtest，并建立 ROS2 launch/topic/service 集成测试。
5. 清理配置与代码不一致项，并对当前 commit 在 ROS2 Humble 重新 build/test/运行验证。
