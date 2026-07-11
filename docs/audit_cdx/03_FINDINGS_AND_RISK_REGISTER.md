# 问题清单与风险登记

## P0：实车前阻断项

### CDX-P0-001：Planning failure/emergency stop 在 Control 入口被丢弃

证据：

- Planning 在 [`planning_node.cpp`](../../src/low_speed_av_planning/src/planning_node.cpp#L405) 发布 `emergency_stop=true` 的 failure trajectory。
- Control 的 [`on_trajectory`](../../src/low_speed_av_control/src/control_node.cpp#L225) 只复制 points，不读取 `msg->emergency_stop` 或 `msg->status`。
- 构造函数默认的 Pure Pursuit 以及 Stanley、MPC 对“单个零速点”仍可返回 enable=true；随包 YAML 默认的 LQR 只对“全部点均零速”做了额外 emergency stop，仍未消费消息级 emergency flag。
- SCU mapper 只有在 internal command 的 `emergency_stop`、`brake>0` 或 `enable=false` 时才发布 brake stop。

影响：当路网已经加载但规划失败时，failure trajectory 通常含一个零速点。未加载 YAML 或切换到 Pure Pursuit/Stanley/MPC 时，路径可能映射为 `target_speed=0`、`brake=false`；任意带非零点的 emergency trajectory 在 LQR 下也可能继续跟踪。它没有兑现“失败/急停轨迹下游必须 SCU brake stop”的合同。

修复：Control 保存 trajectory emergency/status；任一 emergency、failure、空/非法状态立即锁存 controlled stop；增加 Planning→Control→ScuCommandMapper 集成测试。

验收：对 `/planning/trajectory` 发布 `emergency_stop=true` 且包含一个点的消息，四种 controller 下 SCU 均必须 `target_speed=0`、`brake_enable=true`，状态说明同一原因。

### CDX-P0-002：Chassis Driver 没有独立命令超时看门狗

证据：[`control_command_bridge.cpp`](../../src/yunle_chassis/chassis_driver/src/control_command_bridge.cpp#L38) 在每次订阅回调中编码并立即发送；driver 没有缓存最后命令、命令时间、周期 TX timer 或 timeout brake 逻辑。

影响：Control 崩溃、DDS 断链或回调停止后，driver 不会主动下发新的安全帧。最终行为完全依赖底盘硬件自身未在代码合同中证明的 watchdog。

修复：driver 增加独立 20–50 Hz command scheduler、command age watchdog、启动默认 brake、shutdown brake、通信健康状态；硬件 watchdog 周期也要写入协议合同。

验收：运动命令后停止 ROS publisher，超过配置 timeout 必须抓到连续 brake CAN frame，并发布 driver fault/status。

## P1：核心可靠性与完整性

### CDX-P1-001：车辆许可和故障字段没有形成控制门控

`VehicleState.autonomous_enabled` 虽被赋值但不参与 `on_timer()`；`brake_pressed` 和 `fault_code` 甚至没有复制到内部状态。控制可在自治未使能、人工制动或车辆故障时继续生成 tracking command。

建议建立 `WAIT_INPUTS -> READY -> ACTIVE -> CONTROLLED_STOP -> ESTOP_LATCHED` 状态机，并明确每个输入的 required/optional、timeout 和恢复条件。

### CDX-P1-002：latched estop 可被普通 OK 心跳自动清除

[`on_safety_status`](../../src/low_speed_av_control/src/control_node.cpp#L245) 将 `ok`、`standby` 等普通状态视为 clear。默认 `estop_latched=true` 实际不要求显式人工确认，下一条正常 safety 消息即可解除。

建议使用独立 clear service/token，或至少只接受明确 `state=clear` 且满足车辆静止、故障消失、操作者确认的消息。

### CDX-P1-003：默认不发布规范要求的 `/control/command`

代码和两份默认 YAML 都使用 `output.mode=scu_control_command`；[`publish_internal_command`](../../src/low_speed_av_control/src/control_node.cpp#L403) 因此返回 false。仓库规范要求 Control 发布 `/control/command` 和 `/control/status`，当前默认只发 SCU 与 status。

建议默认 `both`，或者修改正式接口合同并明确 `/control/command` 只是 debug；二者必须统一。

### CDX-P1-004：控制参数缺少合法性校验，部分限值参数未生效

Vehicle limits、lookahead、timeout、LQR/MPC 权重和 SCU 参数大多直接读取。负限值可能破坏 `std::clamp` 的前置条件。`max_front_steer_rate_radps`、`max_rear_steer_rate_radps` 已读取但 smoother 只用单一 `command_smoother.max_steer_rate_radps`；accel/decel/jerk 也没有基于真实 `dt` 完整限制。

建议使用 ROS2 parameter descriptor/range 和启动时 fail-fast 校验；把 limiter/smoother 改为以实际周期、前后轮独立 rate、accel/decel/jerk 为输入的可测试状态机。

### CDX-P1-005：manifest.files 可逃逸 AD Package 根目录

[`resolve_file`](../../src/low_speed_av_planning/src/roadnet_loader.cpp#L477) 允许绝对路径或 `../`。checksum 路径有安全检查，但真正用于 LoadFile 的 manifest file 路径没有同等检查。

建议 canonicalize root 和 resolved path，拒绝任何不在 root 下的目标，也拒绝 symlink escape；增加恶意 manifest 负例。

### CDX-P1-006：生产 C++ 基本没有自动化测试

仓库只有一个注册 pytest，它通过 subprocess 调用 Python 版连续性脚本。Control、Simulation、Chassis 没有注册测试；RoadnetLoader、规划器、控制器、SCU mapper 和 DBC codec 都没有直接链接生产 C++ 的 gtest。

影响：Python replica 与 C++ 漂移时，smoke 仍可能通过。本轮发现的 emergency stop 丢失就没有被现有检查捕获。

### CDX-P1-007：当前 commit 缺少同等级 ROS2 回归证据

`reports/ubuntu_ros2_runtime_validation_report.md` 记录 2026-06-10 曾成功构建 7 个包，但当时是 `0 tests`，并发现 task point/trajectory lifecycle 问题。之后仓库又有多轮修复；后续 `UBUNTU_ROS2_REVALIDATION_*` 多数是复测步骤/期望，没有保存完整实测输出。

建议为当前 commit 保存 build/test/test-result、launch、service、topic、bag/日志和 commit SHA。

### CDX-P1-008：底盘 UDP 接收不校验远端来源且缺少通信健康诊断

[`UdpChannel::receive`](../../src/yunle_chassis/chassis_driver/src/udp_channel.cpp#L69) 接收任意源地址数据报，没有与配置的 remote endpoint 比较。driver 也不发布 RX timeout、TX failure count、channel up/down 或 last frame age。

建议验证源 IP/port，统计 malformed/trailing/unknown frame，提供 diagnostics/status，并对 RX/TX fault 触发安全策略。

## P2：功能、算法与一致性

| ID | 问题 | 证据/影响 | 建议 |
|---|---|---|---|
| CDX-P2-001 | `frenet_lite`、`hybrid_astar_parking` 只是 reference-line 别名 | 对应 cpp 只有 include，类直接继承参考线 | 标为 experimental/fallback，或实现真实采样、碰撞与停车状态空间。 |
| CDX-P2-002 | `obstacle_aware` 只是参数 stub | 无 obstacle subscription，只按一个 distance 参数将下游速度清零 | 定义障碍物接口、投影、制动距离和 stale policy。 |
| CDX-P2-003 | Loader 一致性校验不足 | 不拒绝重复 ID、负 cost、count 不一致、range/edge 不一致 | 增加结构校验并复用随包 JSON Schema。 |
| CDX-P2-004 | A* 最优性与确定性没有保证 | heuristic 与 edge cost 尺度无约束；等价队列无 tie-break | 计算 admissible scale，或声明 weighted A*；增加 node/edge ID tie-break。 |
| CDX-P2-005 | Semantic GlobalRoute 摘要不一致 | 追加目标 edge/node，但 length/time 不更新 | 对最终几何段重算 route length/time，或分开 topology route 与 terminal segment。 |
| CDX-P2-006 | 最近点全局搜索可能在回环路线上跳进度 | Planning local crop 和多个 controller 都遍历整个 trajectory | 保存单调 progress，限制搜索窗口并结合 heading/gear。 |
| CDX-P2-007 | 倒车控制没有专用运动学处理 | PP/Stanley/LQR 都按前进误差模型 | 默认继续禁用；补 reverse heading、lookahead、steering sign 和实车前测试。 |
| CDX-P2-008 | Simulation 不是控制闭环仿真 | 它直接沿 planning path 更新 pose，不消费 ControlCommand | 增加 kinematic bicycle plant，区分 path replay 与 control closed-loop。 |
| CDX-P2-009 | Simulation 到达参数无效 | `goal_tolerance_m_`、`yaw_tolerance_rad_` 只赋值未使用 | 真正用于 arrived 判定，并验证 stop profile。 |
| CDX-P2-010 | 算法切换不处理已有轨迹状态 | service 只替换字符串/实例 | 清空缓存或原子重规划，发布明确状态。 |
| CDX-P2-011 | SCU gear 合同不完全一致 | 内部 3=PARK，mapper 只识别 1/2/4 | 明确 PARK 到 N+brake 或拒绝策略，统一枚举。 |
| CDX-P2-012 | Bringup 不是完整系统入口 | demo 只启 Planning+Control，不启 Simulation/Chassis | 提供 sim、bench、vehicle 三种组合 launch 和安全默认值。 |

## P3：维护与治理

| ID | 问题 | 建议 |
|---|---|---|
| CDX-P3-001 | 两个 offline 脚本默认指向已不存在的 `roadnet_ad_package_20260610T012525Z` | 改为 `_2` 或要求必填参数，并在 CI 覆盖默认命令。 |
| CDX-P3-002 | `offline_algorithm_smoke.py` 对空 parking 列表直接 IndexError | 使用 sample fixture 或给出清晰 skip/error。 |
| CDX-P3-003 | Template 与当前实现漂移 | 生成物与 template 建立单一来源或自动同步测试。 |
| CDX-P3-004 | 200 份 Markdown 中历史审计/报告占比过高，旧结论并存 | 标记 archived/superseded，建立 canonical docs index。 |
| CDX-P3-005 | `docs/07_config_launch_runtime.md` 仍列出不存在的 launch 参数 | 用 launch introspection/静态测试生成参数表。 |
| CDX-P3-006 | 无 CI、仓库 LICENSE、CONTRIBUTING、SECURITY、CODEOWNERS | 补工程治理文件和 required checks。 |
| CDX-P3-007 | package 版本均为 0.1.0，maintainer 使用 demo 地址 | 发布前统一版本策略和真实维护信息。 |
| CDX-P3-008 | PlanningNode 约 1800 行，职责集中 | 拆分 anchor resolver、trajectory builder、mission coordinator 和 ROS adapter。 |
