# 规划/控制文档索引

本文列出当前工程中与规划、定位输入、控制输出、代码走读和人工验证相关的中文文档。

## 推荐阅读顺序

1. [规划、定位输入与完整技术路线说明](PLANNING_LOCALIZATION_USAGE_AND_TECHNICAL_ROUTE.md)
   - 先读这份文档，集中了解规划需要准备什么数据、定位如何接入、最终输出是什么，以及 AD Package 到底盘 SCU 命令的完整技术路线。
   - 明确当前工程没有实现独立定位算法节点，控制模块消费外部 `/localization/pose`。

2. [规划/控制模块输入指南](PLANNING_CONTROL_INPUT_GUIDE.md)
   - 了解启动前需要准备哪些 AD Package 文件、参数、topic 和安全输入。

3. [路线规划操作指南](ROUTE_PLANNING_OPERATION_GUIDE.md)
   - 说明如何通过 `/low_speed_av_planning/plan_route` 触发规划。
   - 包含 `ReloadRoadnet`、`PlanRoute`、`SetPlannerAlgorithm` 的实际字段和命令。

4. [技术路线与数据流](TECHNICAL_ROUTE_AND_DATAFLOW.md)
   - 用端到端数据流解释从 AD Package 到 `/yunle_chassis/control/scu_control_command` 的链路。

5. [仿真可视化模块使用说明](SIMULATION_VISUALIZATION_USAGE.md)
   - 说明如何启动 RViz 可视化、模拟 `/localization/pose`，以及如何观察规划路线和轨迹。

6. [规划使用当前定位作为路线起点的调整说明](PLANNING_CURRENT_POSE_START_ADJUSTMENT.md)
   - 说明空 `start_node_id` 的 `PlanRoute` 如何使用当前 `/localization/pose` 推断起点。

7. [定位 Pose 模拟指南](LOCALIZATION_POSE_SIMULATION_GUIDE.md)
   - 说明 fixed pose、trajectory replay 和 roadnet waypoint replay 三种模拟定位模式。

8. [仿真可视化与当前定位起点规划集成流程](SIMULATION_AND_PLANNING_INTEGRATION_FLOW.md)
   - 说明从 AD Package、RViz、模拟定位、PlanRoute 到控制输出的联调流程。

9. [规划模块代码走读](PLANNING_MODULE_CODE_WALKTHROUGH.md)
   - 适合开发和审计人员阅读。
   - 覆盖 RoadnetLoader、Dijkstra/A*、运动规划、速度规划、PlanRoute 回调。

10. [控制模块代码走读](CONTROL_MODULE_CODE_WALKTHROUGH.md)
   - 适合控制开发、底盘联调和安全审计人员阅读。
   - 覆盖控制器、车辆模型、限幅、平滑、SCU mapper、Estop。

11. [ROS2 命令示例](ROS2_COMMAND_EXAMPLES.md)
   - 提供可复制命令。
   - 所有 ROS2 命令需要在真实 ROS2 环境中执行。

12. [操作员启动检查清单](OPERATOR_STARTUP_CHECKLIST.md)
   - 面向现场操作员和测试工程师。
   - 包含启动前、启动后、移动前、规划、控制和结束测试的表格。

## 相关已有文档

- [Yunle SCU 命令输出说明](YUNLE_SCU_COMMAND_OUTPUT.md)
- [LQR 控制器设计说明](LQR_CONTROLLER_DESIGN.md)
- [ROS2 集成测试计划](ROS2_INTEGRATION_TEST_PLAN.md)
- [第三轮全工程审计](audits_3/AUDIT_3_SUMMARY.md)
- [第四轮 SCU/LQR 审计](audits_4/AUDIT_4_SUMMARY.md)
- [完整人工验证流程](audits_4/ROS2_MANUAL_VALIDATION_PROCEDURE.md)
- [仿真可视化与当前定位起点规划报告](../reports/simulation_visualization_and_current_pose_planning_report.md)

## 快速答案

### 规划需要什么输入？

- `roadnet.package_path`
- AD Package v1.1 canonical 文件
- `PlanRoute` 服务请求中的起终点
- planner YAML 参数

### 定位模块怎么接入？

当前仓库没有实现独立定位算法节点。外部定位模块需要持续发布：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

控制节点订阅该 topic，并结合 `/planning/trajectory` 计算跟踪命令。

### 怎样触发路线规划？

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

### 控制需要什么输入？

- `/localization/pose`
- `/planning/trajectory`
- `/vehicle/state`
- `/safety/status`
- control YAML 参数

### 最终底盘输出是什么？

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

速度单位为 km/h，转角单位为 deg，挡位只允许 D/N/R 对应 1/2/3。安全停车、急停、定位超时、轨迹超时和无效命令都会映射为制动停车命令。
