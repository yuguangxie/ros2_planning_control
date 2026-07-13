# Phase 18 Control Closed-Loop Simulation Report

## 1. 结论与基线边界

Phase 18 的本地 Ubuntu 22.04 / ROS2 Humble 实现与验证通过，但阶段整体状态是：

```text
LOCAL_IMPLEMENTATION_AND_FULL_VALIDATION_PASS / SAME_SHA_CI_NOT_EXECUTED
```

- baseline branch/SHA：`codex/dev` / `6b07d6cd5ad3ee62f1fdd83dd2fabd6a6ae28da9`。
- Phase 17 本地 7/7 build、active launch 和 sanitizer 已通过，但这些修复与 Phase 18 仍在同一未提交工作区。
- baseline SHA 的 GitHub Actions [run 29185828794](https://github.com/yuguangxie/ros2_planning_control/actions/runs/29185828794) 仍是 `EXECUTED_FAIL`。本任务没有明确 commit/push 授权，因此不存在 Phase 18 同 SHA CI 或 artifact，`CDX-P1-007` 不能关闭。
- 没有修改正式 Roadnet 或 `src/yunle_chassis/**`，没有启动 Chassis Driver/keyboard control，没有打开真实 UDP/CAN、连接 `192.168.*` 或发送 CAN 0x121。

验证根目录：

```text
/home/xie/planning_control/codex_validation/ros2_planning_control_6b07d6c_phase18_20260713_085939
```

## 2. 实现范围与文件

Phase 18 新增/修改集中在：

- Simulation production core、node、config、RViz、CMake/package metadata、C++/launch tests、README；
- Bringup closed-loop launch、internal-only Control override、SIL integration test、README；
- workflow 的 headless SIL、metrics/log artifact 和 Simulation sanitizer step；
- expected-tree/template checks、runtime docs、Phase 18 matrix、audit/final/Ubuntu reports。

Phase 17 的 bringup fixture、Humble exit API、rosdep 和 Simulation reset 变更在开始 Phase 18 前已存在，本报告不把它们重新归因于 Phase 18。

## 3. Production plant

新增共享 production target：

```text
low_speed_av_simulation_core
```

ROS-independent core 文件：

- `simulation_types.hpp`
- `kinematic_vehicle_plant.hpp/.cpp`
- `simulation_runtime_monitor.hpp/.cpp`

中点/RK2 模型：

```text
x_dot   = v * cos(yaw)
y_dot   = v * sin(yaw)
yaw_dot = v * (tan(front_steer) - tan(rear_steer)) / wheel_base
```

假设与边界：低速平面运动、无轮胎侧偏/坡度/动力系统/碰撞 plant；front Ackermann 后轮转角为 0，dual Ackermann 使用实际前后命令；仅 DRIVE 正向运动。速度、accel/decel、optional jerk、前后转角/rate、normal/emergency brake 均受限。非法 dt/options、NaN/Inf、unsupported gear、reverse、disable、brake、emergency 和 command timeout 全部 fail closed。

## 4. ROS runtime 与 launch

`sim_localization_pose_publisher_node` 保留两种明确模式：

- `path_replay`：Planning 快速检查，直接沿 path 回放；
- `control_closed_loop`：pose 只从 production plant state 更新，Planning path 只用于 goal 与 tracking metrics。

闭环链路：

```text
Planning -> Control -> /control/command -> Simulation plant
         <- /localization/pose + /vehicle/state <-
```

闭环发布 `/localization/pose`、`/vehicle/state`、`/simulation/status`、`/simulation/diagnostics`、`/simulation/pose_path`。reset 清空 plant、history、monitor、command cache；start/pause/reset 保留 bounded service 行为。goal position/yaw tolerance 进入 production `goal_reached()`。

新增 `planning_control_closed_loop_sim.launch.py`，只启动 Planning、Control、Simulation plant、Roadnet visualization、optional RViz。`control_sim_params.yaml` 强制 `output.mode=internal` 与 `vehicle_state.required=true`，不改变生产 Control 默认 `both`。闭环专用 Planning override 关闭重复 local crop，避免每次生成新 trajectory identity；Control production progress window 继续按 pose 裁剪。

## 5. Production-linked C++ tests

`test_simulation_core` 直接链接 `low_speed_av_simulation_core`：25/25 PASS。

| Suite | Cases | PASS | Coverage |
|---|---:|---:|---|
| Plant | 22 | 22 | straight/curvature, front/dual Ackermann, accel/decel/jerk, front/rear rate, brake/emergency/disable, invalid dt/options/NaN/gears/reverse, timeout/reset/determinism, goal/yaw tolerance |
| Monitor | 3 | 3 | fake/explicit cadence, max/p95/RMS errors, timeout/non-finite counters, reset |

所有时间相关 core cases 使用显式 dt，没有长 sleep，也没有复制 production plant 算法。

## 6. ROS2 integration

Simulation node tests：4 active methods（含两份 post-shutdown）全部 PASS：ControlCommand 驱动 pose、Plant VehicleState、0.2 s timeout 停车、reset/history、diagnostics、internal-only/no Chassis、bounded exit。

Full SIL：5 active runtime methods + 1 post-shutdown 全部 PASS：

- materialized sample ready，PlanRoute `N0001 -> N0003` 成功；
- Control ACTIVE，command 实际驱动 plant pose；
- Pure Pursuit、Stanley、LQR、MPC sampler × front/dual Ackermann 均产生有限闭环输出；
- goal/yaw tolerance 到达并停车；
- Planning invalid goal 产生 emergency ControlCommand 并使运动中的 plant 停车；
- 没有 SCU 实际消息、Chassis 或 keyboard 节点；进程 clean exit。

既有 Control/Planning launch tests继续覆盖 localization/trajectory/VehicleState timeout、disabled/brake/fault、estop latch/clear、switch READY/reset、late subscriber。Chassis publisher loss 仍为 1 个 `SKIPPED_KNOWN_PRODUCTION_GAP`。

## 7. SIL metrics

最终 headless run 的 `sil_metrics.json`：

| Metric | Actual | Gate |
|---|---:|---:|
| Control interval max / p95 | 0.0206 / 0.0202 s | max < 0.15 s |
| Localization interval max / p95 | 0.0501 / 0.0500 s | max < 0.15 s |
| Plant step interval max / p95 | 0.0101 / 0.0100 s | observation |
| Lateral error max / RMS / p95 | 0.4056 / 0.2745 / 0.3723 m | max < 0.8, RMS < 0.4 m |
| Heading error max / RMS / p95 | 0.3822 / 0.1742 / 0.0322 rad | observation |
| Goal position / yaw error | 0.2855 m / 0.0322 rad | < 0.3 m / < 0.35 rad |
| Stopped speed | 0.0250 m/s | < 0.05 m/s |
| Stop response | 0.6100 s | observation; SIL plant only |
| Non-finite count | 0 | = 0 |
| Timeout count | 1 | expected startup waiting-for-first-command episode |

横向误差使用到最近 path segment 的投影，不把稀疏 waypoint 间的纵向距离误记为 lateral error。

## 8. Full build/test 与 sanitizer

| Gate | Actual |
|---|---|
| Offline | 18/18 PASS |
| rosdep check | PASS，标准 metadata |
| Full build | 7/7 packages PASS |
| Full colcon test/result | RC=0 / RC=0；aggregate 147，0 failure/error，1 known-gap launch skip |
| Planning C++ | 40/40 PASS |
| Control C++ | 28/28 PASS |
| Simulation C++ | 25/25 PASS |
| Chassis core | 7 active PASS；4 watchdog specs 保持 known-gap SKIP |
| ASan/UBSan | build PASS；100 active production C++ PASS；4 Chassis known-gap SKIP；无 sanitizer error |

完整日志：`final/log`、`final/build/*/test_results`、`final/ros_log`、`asan/log`、`asan/build/*/test_results`。

## 9. GitHub Actions

Workflow 已配置：Jammy/Humble full build/test、显式 headless Simulation+Bringup SIL timeout、ROS logs/SIL metrics artifact、Simulation core ASan/UBSan。状态只能写：

```text
CONFIGURED_NOT_EXECUTED
```

原因是工作区未 commit/push，不能把 baseline SHA 的失败 run 或本地结果写成同 SHA CI PASS。

## 10. Findings

| Finding | Phase 18 状态 | 判断 |
|---|---|---|
| `CDX-P2-008` | `FIXED_LOCAL_SIL / CI_PENDING` | production ControlCommand 实际驱动 production plant，本地 C++/ROS SIL PASS |
| `CDX-P2-009` | `FIXED_LOCAL_SIL / CI_PENDING` | goal/yaw tolerance 实际参与 arrived，goal stop 与 metrics PASS |
| `CDX-P2-012` | `PARTIALLY_FIXED_SIM_ENTRY` | 完整安全 sim entry/RViz/docs 已有；bench/vehicle entry 不在本阶段 |
| `CDX-P2-007` | `OPEN_CAPABILITY / FAIL_CLOSED_MITIGATION` | reverse 只停车，不实现 reverse controller |
| `CDX-P1-007` | `OPEN / SAME_SHA_CI_NOT_EXECUTED` | 无 Phase 17/18 新 commit 的 CI PASS |
| `CDX-P0-002` | `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION` | SIL timeout 不是 Chassis software watchdog |
| HIL | `HIL_NOT_EXECUTED` | SIL 不验证 500 ms hardware watchdog |

## 11. Known limitations 与 handoff

- 该 plant 是确定性低速 kinematic SIL，不是闭环动力学、轮胎/制动标定、碰撞仿真、HIL 或实车证据。
- reverse、高级 Planning、完整 MPC、Frenet、Hybrid A*、Chassis software watchdog 均未进入本阶段。
- `CDX-P1-007` 与 Phase 17 CI 边界仍阻止“整体完成”结论。

唯一 handoff：经用户明确授权后，将现有 Phase 17+18 工作区整理为可审查 commit 并推送，等待同一 SHA 的 `build-test`、`sanitizers` 和 artifacts 全部 PASS；在此之前不要宣称 Phase 18 整体 CI 完成，也不要进入 HIL。
