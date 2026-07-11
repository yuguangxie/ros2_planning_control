# 架构与模块审计

## 实际技术路线

```text
project_manifest.json + topology.json + waypoints.yaml + semantics
  -> RoadnetLoader / RoadnetPackage
  -> TopologyGraph
  -> Dijkstra 或 A*
  -> edge sequence
  -> ReferenceLine + semantic anchor stitching
  -> constant / curvature / obstacle_aware speed planning
  -> /planning/full_reference_path + /planning/trajectory
  -> Pure Pursuit / Stanley / LQR / MPC sampler
  -> front_ackermann / dual_ackermann
  -> limiter + smoother
  -> ScuCommandMapper
  -> /yunle_chassis/control/scu_control_command
  -> chassis_driver UDP-CAN
```

主路线与仓库要求一致，Planning 和 Control 没有互相包含对方实现。Simulation 复用 Planning 的 RoadnetLoader；Chassis 通过独立消息包与 Control 对接。

## 模块完整性矩阵

| 模块 | 已实现 | 部分实现/缺口 | 结论 |
|---|---|---|---|
| `low_speed_av_interfaces` | 7 个内部 msg、5 个 srv；字段与 `docs/03_ros2_interfaces.md` 基本一致 | 没有 action；状态和枚举大量使用自由字符串；缺少接口级兼容测试 | 基本完整 |
| `low_speed_av_planning` | loader、checksum、validation、拓扑、Dijkstra/A*、语义点、no-go/speed-zone、全量/局部轨迹、失败轨迹、算法工厂 | 高级 motion 是 reference-line 别名；obstacle-aware 无障碍物订阅；节点过于集中；若干参数无效 | 原型可用 |
| `low_speed_av_control` | 4 控制器、2 Ackermann 模型、限幅、平滑、timeout、safety status、SCU 映射 | 急停轨迹未消费；自治/故障未门控；速率/加减速度限制不完整；默认不发布内部命令 | 存在安全缺口 |
| `low_speed_av_bringup` | Planning+Control launch、两套参数、sample AD Package | 未一键启动 Simulation/Chassis；offline tools 不在包内安装；配置含大量无效键 | 部分完整 |
| `low_speed_av_simulation` | Roadnet/route/trajectory/vehicle marker；定位路径回放；pause/reset/rewind | 不是车辆动力学模型；不消费 ControlCommand；目标容差参数未生效；终点停车是回放器瞬时截停 | 演示工具 |
| `chassis_interfaces` | 11 个底盘消息，SCU 命令合同明确 | 无诊断/通信状态消息；枚举和单位主要靠注释 | 可集成 |
| `chassis_driver` | 双 UDP 通道、13-byte codec、硬编码 DBC、typed feedback、SCU/torque/chassis/debug TX | 无命令 watchdog、无远端源校验、无通信健康 topic、无周期安全发送、无单测 | 台架前需补强 |

## AD Package 加载与规划

Loader 的正向实现较完整：

- 从 [`roadnet_loader.cpp`](../../src/low_speed_av_planning/src/roadnet_loader.cpp#L245) 的 `project_manifest.json` 开始。
- 支持 `schema=low_speed_roadnet_ad_package` 和 `1.1.x`。
- 读取 coordinate system、units、manifest.files、manifest.hashes。
- 同时检查 manifest validation 与 `validation/validation_report.json`。
- 将 `end_index` 转换为内部半开区间，优先支持 `end_index_exclusive`。
- C++ 内置 SHA-256，并比较 checksums 与 manifest hashes。
- no-go area 转换为 blocked edge，speed zone 对轨迹速度限幅。

主要结构性不足：

- [`resolve_file`](../../src/low_speed_av_planning/src/roadnet_loader.cpp#L477) 直接拼接 manifest 路径，没有 canonical/absolute 路径约束。
- 没有验证重复 node/edge/waypoint ID、负 cost/length、range count 与区间长度、range 内 waypoint 的 edge_id。
- 随包携带的 JSON Schema 没有在运行时使用，当前只是手工字段检查。
- `RoadnetStatus.validation_status` 在 ready 时固定写成 `passed`，会丢失正式包真实的 `warning` 状态。

## 全局、运动与速度规划

Dijkstra 和 A* 都是真实有向图搜索，并支持 disabled、blocked、reverse 过滤。参考线运动规划会按 waypoint index 拼接、去重、重算 route s，并支持 horizon。

完整性差异需要明确：

- `reference_line`：真实实现。
- `stop_and_wait`：安全停车实现。
- `frenet_lite`：仅继承 `ReferenceLineMotionPlanner`，没有 Frenet 采样或碰撞评价。
- `hybrid_astar_parking`：仅继承参考线，没有 Hybrid A* 状态空间搜索。
- `constant`、`curvature`：真实实现。
- `obstacle_aware`：只读取参数中的一个障碍物距离，没有 obstacle topic、预测或制动曲线。

A* 使用欧氏距离作为 heuristic，但 edge cost 未保证与米制距离同尺度，因此不保证任意配置下仍为最优路径。等价代价时 priority queue 也没有稳定 tie-break，不满足严格的跨平台确定性要求。

## 语义目标与轨迹生命周期

项目实现了 node/task/parking/charging/current_pose anchor，支持 semantic point 投影到 edge waypoint，并将 full reference path 与局部 trajectory 分开。该设计适合长路线持续跟踪，也是当前 Planning 最成熟的部分之一。

仍有以下问题：

- `route_with_goal_edge_for_message()` 会把语义目标 edge/node 附加到消息，却不更新 `length_m` 和 `estimated_time_s`，路线摘要内部不一致。
- 局部轨迹和控制器均使用“全轨迹最近点”搜索，交叉、回环或相邻平行边可能发生进度跳变。
- 失败轨迹使用 package 第一个 waypoint 作为点坐标，而不是当前定位；虽然标了 emergency stop，但会增加下游误用风险。
- 算法切换不会清空或重算现有 cached route/trajectory，状态语义不完整。

## 控制器与车辆模型

Pure Pursuit、Stanley、LQR 均有可执行逻辑。LQR 使用离散运动学误差模型和 Riccati 迭代，不再只是固定增益。MPC sampler 是确定性固定曲率采样器，但只用恒定当前速度 rollout，且用固定 horizon 对应的单个参考点计价，不能等同于完整 MPC。

Front/Dual Ackermann 公式符合仓库约定。SCU mapper 将 m/s 转 km/h、rad 转 deg，并按 gear 选择 D/R/N。

倒车路径尚没有倒车专用横向误差和预瞄规则。当前控制器仍按前进几何求最近点、heading error 和 steering，开启 reverse planning 前必须做单独验证。

## 仿真模块性质

Simulation 的 `path_follow` 按规划几何和目标速度移动 `/localization/pose`，并不是订阅 `/control/command` 后积分车辆模型。因此它可以验证 Roadnet—Planning—Pose 数据流，却不能验证控制器是否真的把车辆拉回轨迹，也不能发现 steering 符号、车辆模型或 actuator dynamics 问题。

`goal_tolerance_m` 和 `yaw_tolerance_rad` 已读取但没有用于到达判定。轨迹末点零速会被 `speed_for_progress()` 替换为默认速度，最后到 endpoint 时再把速度置零，仿真停车曲线不真实。

## 底盘链路

底盘驱动的 DBC、CAN Ethernet codec 和反馈 topic 是实际实现，Control 到 SCU 的 topic 名与字段可对接。主要问题不是“有没有驱动”，而是“驱动缺少最后一道独立安全层”：每个 ROS 命令回调只发送一次 CAN frame；如果 Control 进程停止，driver 不会主动生成 brake frame，也没有暴露 command age、RX age 或 channel health。
