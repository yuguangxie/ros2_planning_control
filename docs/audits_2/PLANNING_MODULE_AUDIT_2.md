# 规划模块第二轮审计

## Objective
审计优化后的 planning package 是否具备运行时服务、全局规划、轨迹拼接、速度规划、状态发布和失败行为，并与第一轮 P0/P1 问题对比。

## Status: Partial
源码层面已实现规划节点 runtime pipeline；由于未运行 ROS2，service callback 实际可调用性、接口生成和 topic 发布未做 runtime 验证。

## Evidence
- `ReloadRoadnet` 服务：`src/low_speed_av_planning/src/planning_node.cpp:56` 至 `src/low_speed_av_planning/src/planning_node.cpp:60`。
- `PlanRoute` 服务：`src/low_speed_av_planning/src/planning_node.cpp:63` 至 `src/low_speed_av_planning/src/planning_node.cpp:67`。
- `SetPlannerAlgorithm` 服务：`src/low_speed_av_planning/src/planning_node.cpp:70` 至 `src/low_speed_av_planning/src/planning_node.cpp:74`。
- route/trajectory publisher：`src/low_speed_av_planning/src/planning_node.cpp:47` 至 `src/low_speed_av_planning/src/planning_node.cpp:50`。
- global planner factory 使用：`src/low_speed_av_planning/src/planning_node.cpp:207`。
- motion/speed planner factory 使用：`src/low_speed_av_planning/src/planning_node.cpp:216` 至 `src/low_speed_av_planning/src/planning_node.cpp:218`。
- 成功发布 route 和 trajectory：`src/low_speed_av_planning/src/planning_node.cpp:355`、`src/low_speed_av_planning/src/planning_node.cpp:371`。
- Dijkstra/A* 工厂支持：`src/low_speed_av_planning/src/global_planner_factory.cpp:10` 至 `src/low_speed_av_planning/src/global_planner_factory.cpp:16`。
- motion planner 工厂支持四类算法：`src/low_speed_av_planning/src/reference_line_motion_planner.cpp:73` 至 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:84`。
- speed planner 工厂支持三类算法：`src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:21` 至 `src/low_speed_av_planning/src/obstacle_aware_speed_planner.cpp:29`。

## Findings
### A2-PL-001：规划节点服务表面已补齐
- Severity: P0
- Status: Pass by static audit / Not Verified by ROS2 runtime
- Impact on planning/control/vehicle operation: 控制模块现在有机会接收规划发布的 route/trajectory。
- Recommended fix: 在 ROS2 环境中调用 `PlanRoute`，并检查 `/planning/global_route` 与 `/planning/trajectory`。
- Verification method: service call `N0001 -> N0003`，断言非空 route/trajectory。

### A2-PL-002：算法工厂已接入节点运行路径
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: YAML 或 service 切换算法后，规划节点将使用指定 factory。
- Recommended fix: 增加配置行为测试，覆盖 `dijkstra/astar`、blocked_edges、speed planner。
- Verification method: 修改 `global_planner.blocked_edges` 后输出 route 应变化或失败。

### A2-PL-003：失败路径发布 failure status 和停车轨迹
- Severity: P1
- Status: Pass by static audit
- Impact on planning/control/vehicle operation: 无法规划时下游不会收到误导性的有效高速轨迹。
- Recommended fix: ROS2 runtime 验证异常和无路由分支。
- Verification method: 请求不存在节点或阻塞全部边，检查 status=failure 和 stop trajectory。

### A2-PL-004：motion planner skeleton 仍需成熟化
- Severity: P3
- Status: Partial
- Impact on planning/control/vehicle operation: `frenet_lite`、`hybrid_astar_parking` 和 `stop_and_wait` 不应被误认为完整算法。
- Recommended fix: `stop_and_wait` 输出明确停车轨迹；文档/配置标记 skeleton；补测试。
- Verification method: 四类 motion planner factory 输出行为测试。

### A2-PL-005：obstacle_aware 仍是简化 stub
- Severity: P3
- Status: Partial
- Impact on planning/control/vehicle operation: 没有真实 obstacle topic/list 输入，动态障碍物不会被完整处理。
- Recommended fix: 定义 obstacle 输入接口和速度规划行为。
- Verification method: 障碍物距离输入影响目标速度。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /low_speed_av_planning/plan_route ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/global_route`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/trajectory`
- SKIPPED_ROS2_UNAVAILABLE: `colcon build --packages-select low_speed_av_planning`

