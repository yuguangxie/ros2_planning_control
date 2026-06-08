# 规划模块审计

## 目标
审计全局规划、运动规划、速度规划、工厂模式、轨迹拼接和规划节点集成情况。

## 状态
部分通过。

## 证据
- Dijkstra 工厂分支见 `src/low_speed_av_planning/src/global_planner_factory.cpp:12`。
- A* 工厂分支见 `src/low_speed_av_planning/src/global_planner_factory.cpp:15`。
- Dijkstra 对 enabled、blocked、reverse 过滤见 `src/low_speed_av_planning/src/dijkstra_planner.cpp:10` 至 `src/low_speed_av_planning/src/dijkstra_planner.cpp:16`。
- A* 启发式距离使用见 `src/low_speed_av_planning/src/astar_planner.cpp:44`。
- reference_line 使用 waypoint range 拼接，见 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:28` 至 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:40`。
- reference_line 支持按 pose 裁剪，见 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:42` 至 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:52`。
- reference_line 重算 route `s_m`，见 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:54` 至 `src/low_speed_av_planning/src/reference_line_motion_planner.cpp:58`。
- `stop_and_wait` 当前返回空轨迹，见 `src/low_speed_av_planning/include/low_speed_av_planning/stop_and_wait_motion_planner.hpp:10` 至 `src/low_speed_av_planning/include/low_speed_av_planning/stop_and_wait_motion_planner.hpp:12`。
- 规划节点只加载包并发布状态，见 `src/low_speed_av_planning/src/planning_node.cpp:17` 至 `src/low_speed_av_planning/src/planning_node.cpp:67`。

## 发现
### F-PL-001：Dijkstra 和 A* 核心逻辑存在
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：全局规划算法可以在 ROS2 外进行单元测试。
- 推荐修复：补充 C++ 单元测试，覆盖 blocked edge、reverse、断连图和 A*/Dijkstra 结果一致性。
- 验证方法：构造小图测试不同权重和阻塞条件。

### F-PL-002：规划节点不生成路线和轨迹
- 严重级别：P0
- 状态：失败
- 对规划/控制/车辆运行影响：`/planning/global_route` 和 `/planning/trajectory` 不会从任务请求中生成，控制模块无法获得参考轨迹。
- 推荐修复：实现 `PlanRoute`、`ReloadRoadnet`、`SetPlannerAlgorithm` 服务；从配置实例化工厂；发布 `GlobalRoute`、`Trajectory` 和失败状态。
- 验证方法：ROS2 环境中调用 `PlanRoute N0001 N0003`，断言 route/trajectory 非空；无 ROS2 时用 C++ 纯逻辑测试同一 pipeline。

### F-PL-003：规划配置未接入算法参数
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：YAML 中 blocked_edges、heuristic_weight、horizon、speed planner 选择等不会影响运行时规划节点。
- 推荐修复：将参数读取到 `GlobalPlannerOptions`、`MotionPlannerOptions`、`SpeedPlannerOptions`，并在规划请求中使用。
- 验证方法：修改配置为 `dijkstra`、阻塞边、缩短 horizon，输出应发生可预期变化。

### F-PL-004：部分 motion planner 是骨架且行为不明确
- 严重级别：P3
- 状态：部分通过
- 对规划/控制/车辆运行影响：选择 `stop_and_wait` 会得到空轨迹；`frenet_lite` 和 `hybrid_astar_parking` 当前只是复用 reference-line 行为。
- 推荐修复：在代码和配置中标明 skeleton/TODO；`stop_and_wait` 至少应输出可控停车轨迹，而不是空轨迹。
- 验证方法：对四个 motion planner 做 factory 选择和输出行为测试。

### F-PL-005：obstacle_aware 速度规划只是近距离停车 stub
- 严重级别：P3
- 状态：部分通过
- 对规划/控制/车辆运行影响：没有 obstacle topic 或障碍物列表输入；只根据 `obstacle_distance_m` 选项停车。
- 推荐修复：保留 stub 但明确输入合同，后续接入障碍物数据。
- 验证方法：设置 `obstacle_distance_m <= obstacle_stop_distance_m`，检查下游轨迹速度变 0。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /planning/plan_route ...`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/global_route`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 topic echo /planning/trajectory`
