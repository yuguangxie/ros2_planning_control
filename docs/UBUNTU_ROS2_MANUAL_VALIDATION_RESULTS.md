# Ubuntu ROS2 人工验证结果

日期：2026-06-10

验证环境：Ubuntu + ROS2 Humble。执行命令前已退出/规避 conda 环境，清理了 conda/anaconda 相关 `PATH` 和环境变量。本次只做运行阶段验证，没有修改源码、接口、launch、配置或路网数据。

完整报告：

- `reports/ubuntu_ros2_runtime_validation_report.md`
- `reports/simulation_planning_control_validation_report.md`
- 原始日志：`reports/runtime_validation_logs/`

## 可用功能

- ROS2 构建通过，7 个包成功 build：`low_speed_av_interfaces`、`chassis_interfaces`、`chassis_driver`、`low_speed_av_planning`、`low_speed_av_control`、`low_speed_av_simulation`、`low_speed_av_bringup`。
- `rosdep install --from-paths src --ignore-src -r -y` 通过，未发现缺失依赖。
- ROS2 interfaces 可通过 `ros2 interface show` 正常查看。
- 当前 roadnet 包可离线校验，也可被 planning node 加载。
- 仿真定位 `/localization/pose` 持续发布，约 20 Hz，`frame_id: map`，四元数有效。
- roadnet、vehicle、route、trajectory 的 Marker/Path topic 均有非空数据。
- 基于路网 node id 的规划可用，已验证相邻节点、多 edge 路线、反向 edge 路线。
- 空起点规划可用，planner 能使用当前 `/localization/pose` 作为规划起点。
- `ReloadRoadnet` 服务可用，返回 `success=True` 和 `roadnet ready`。
- SCU 输出 topic 名称和类型正确：`/yunle_chassis/control/scu_control_command`，`chassis_interfaces/msg/ScuControlCommand`。
- 前进和倒车 SCU 映射基本正确：D 档为 1，R 档为 3，速度为非负 km/h。
- 安全失败路径可用：无效 node、无效 task point、无效 parking point、定位超时均发布安全停车轨迹。
- safety estop 可用：control 进入 `safety_estop`，SCU 输出 speed=0、brake=true。

## 不可用功能

- 当前 roadnet 包中的真实任务点规划不可用。使用 `RP-001`、`RP-003` 等真实 task point ID 调用规划时，服务返回失败。
- 停车点成功规划不可验证，因为 `semantics/parking_points.json` 中 `parking_points` 为空。
- 本次没有连接真实底盘，也不建议基于本次结果直接连接真实底盘做运动测试。

## 部分可用功能

- RViz 进程可启动并完成 OpenGL 初始化，但本次没有人工截图/目视确认 RViz 画面内容；已用 topic/MarkerArray/Path 做 headless 验证。
- control 能收到 planning 轨迹并短时间输出非零 SCU 命令，但约 0.5 秒后进入 `trajectory_timeout` 并安全停车。原因是当前 planning service 只发布一次轨迹，而 control 使用 `controller.trajectory_timeout_s=0.5` 判定轨迹是否新鲜。
- `/planning/roadnet_status` 可发布 ready 状态，但如果在发布后才执行 `ros2 topic echo --once`，可能收不到一次性消息；提前 echo 后触发 reload 可以正常捕获。

## 关键问题

| ID | 等级 | 问题 | 影响 |
|---|---|---|---|
| UVR-P1-001 | P1 | 真实 task point 规划失败。疑似 `linked_node_id: null` 被解析为非空字符串，导致没有 fallback 到 `linked_edge_id`。 | 任务点任务不可执行。 |
| UVR-P1-002 | P1 | 控制持续跟踪只部分可用。规划成功后 SCU 短暂输出，随后因 trajectory timeout 安全停车。 | demo 无法持续跟踪完整路线。 |
| UVR-P2-001 | P2 | 当前包没有 parking point。 | 停车点规划成功路径未知。 |
| UVR-P2-002 | P2 | roadnet validation 有 32 个非阻塞 warning，包含高曲率和曲率连续性问题。 | 实车低速跟踪风险升高。 |
| UVR-P3-001 | P3 | `ros2 --version` 在当前 Humble CLI 下不是有效版本命令。 | 自动验证脚本可能出现噪声失败。 |
| UVR-P3-002 | P3 | `/planning/roadnet_status` 不是 transient/local 或周期发布，晚订阅容易错过。 | 操作员可能误判 roadnet 状态未发布。 |

## 下一步优先修复建议

1. 修复语义点 null 字段处理：`linked_node_id: null` 应按空值处理，并正确 fallback 到 `linked_edge_id` 对应 edge 的 `to_node_id`。
2. 明确 planning/control 的轨迹生命周期：选择周期重发轨迹、controller 持有最近有效轨迹，或调整 timeout 语义。
3. 准备包含真实 parking point 的测试 roadnet 包，重新验证停车点规划和当前定位到停车点规划。
4. 对当前 roadnet 的高曲率和曲率连续性 warning 做平滑或限速处理。
5. 考虑让 `/planning/roadnet_status` 使用 transient local QoS 或周期发布，方便人工验证和运维观察。

## 是否建议连接真实底盘

不建议连接真实底盘做运动测试。

当前结果支持继续做 bench-only、wheels-off、message-monitor 级验证，但真实车辆运动测试应等待以下问题解决后再考虑：

- 任务点规划成功路径通过；
- 停车点规划成功路径通过；
- control 持续跟踪行为明确且通过验证；
- roadnet 曲率 warning 已处理或已通过低速安全策略覆盖；
- SCU 与真实 chassis driver 的台架级安全验证完成。
