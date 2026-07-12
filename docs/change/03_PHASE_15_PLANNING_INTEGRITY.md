# Phase 15：Planning完整性与安全加固

## 1. 阶段定位

Phase 15在完全不修改`src/yunle_chassis`和Control生产代码的前提下，集中处理Planning输入可信度、图搜索正确性、semantic/trajectory逻辑可测试性和Planning ROS2回归。

阶段状态为`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI`：生产源码、CMake和测试源码已经完成，但本地没有ROS2/C++工具链，新测试尚未实际编译执行。

## 2. AD Package路径安全

主要修改：

- [RoadnetLoader](../../src/low_speed_av_planning/src/roadnet_loader.cpp)
- [Loader测试](../../src/low_speed_av_planning/test/test_roadnet_loader.cpp)

新增统一`resolve_contained_path`逻辑：

1. canonicalize AD Package根目录；
2. 将反斜杠等混合分隔符标准化；
3. 拒绝POSIX、Windows盘符和UNC绝对路径；
4. 拒绝`..`逃逸；
5. 使用`weakly_canonical`检查symlink解析后的真实目标；
6. 确保最终路径仍位于canonical package root内。

manifest files、manifest hashes、checksums和最终文件读取共用同一helper。

具体作用是堵住“checksum检查的是包内文件，但实际LoadFile读取了包外路径”的不一致，防止恶意manifest读取任意主机文件。

## 3. Loader结构和数值fail-closed

在原schema、version、validation和checksum检查基础上，新增拒绝：

- 重复node、edge、path、waypoint和semantic ID；
- edge引用不存在的from/to node；
- 非有限或负cost、length、speed；
- waypoint坐标、yaw、kappa、s、v中的NaN/Inf；
- waypoint index的start/end/count不一致；
- index越界、重叠、coverage缺口或edge_id不一致；
- manifest hashes与checksums冲突；
- validation failed或blocking errors；
- 不兼容schema/version。

具体作用是让损坏Roadnet在加载阶段明确失败，而不是进入图搜索或轨迹拼接后产生更难诊断的越界、非确定结果或非法速度。

坏包测试全部在临时目录构造，不修改sample和正式Roadnet数据。

## 4. Dijkstra与A*正确性

主要修改：

- [TopologyGraph](../../src/low_speed_av_planning/src/topology_graph.cpp)
- [Dijkstra](../../src/low_speed_av_planning/src/dijkstra_planner.cpp)
- [A*](../../src/low_speed_av_planning/src/astar_planner.cpp)

### 4.1 统一成本合同

负cost和非有限cost在Loader和planner两层拒绝。这样即使测试或其他调用方手工构造图绕过Loader，planner也不会在无效成本上运行。

### 4.2 Admissible A* heuristic

A*使用全图最小`edge.cost / endpoint_distance`缩放直线距离，使默认权重1下的heuristic不超过实际剩余成本。权重大于1时明确属于weighted A*，不保证最优。

具体作用是保证默认配置下A*与Dijkstra可以得到相同最优cost，而不会因为成本量纲和几何距离不一致破坏A*最优性。

### 4.3 确定性tie-break

Dijkstra和A*对等价cost使用稳定ID顺序进行tie-break。

具体作用是让相同输入在不同运行、不同容器或不同STL堆实现下保持可重复路径，便于回归、审计和问题复现。

### 4.4 边界行为

明确覆盖：

- `start == goal`；
- unreachable；
- disabled/blocked edge；
- reverse disabled；
- equal-cost determinism；
- A*/Dijkstra cost equivalence。

## 5. Semantic与Trajectory production helper

新增：

- [planning_helpers.hpp](../../src/low_speed_av_planning/include/low_speed_av_planning/planning_helpers.hpp)
- [planning_helpers.cpp](../../src/low_speed_av_planning/src/planning_helpers.cpp)

将原来深埋在PlanningNode private逻辑中的纯计算提取到production library，Node与gtest调用同一实现。覆盖：

- current pose anchor；
- task/parking/charging的`linked_node`；
- `linked_edge` fallback；
- `null`、字符串`"null"`和none不作为node ID；
- same-edge forward；
- goal behind且reverse disabled；
- final semantic stop；
- terminal local segment；
- route length/time重算；
- `route_s_m`单调；
- full reference与local trajectory终点一致；
- continuity与最大point jump。

具体作用是降低PlanningNode职责集中度，并让semantic goal和轨迹构造能够直接进行production C++单测。

## 6. Route summary与轨迹一致性

GlobalRoute的length/time改为按实际full reference geometry重算。追加semantic terminal edge或局部终点后，不再保留只覆盖topology edge sequence的旧统计。

具体作用是确保：

- GlobalRoute摘要；
- full reference path；
- 发布给Control的local trajectory；

在路径长度、累计`s`和最终终点上使用同一几何事实，避免上层任务系统看到“route已到终点”，而轨迹实际还包含额外terminal segment。

## 7. Planning progress/crop

新增带状态的progress tracker：

- 使用trajectory identity；
- 保存单调progress index；
- 只在有限窗口内搜索；
- 结合heading过滤；
- package reload、cache clear和algorithm switch时reset。

具体作用是避免回环道路上全局最近点搜索突然匹配到几何上靠近、拓扑上却属于后续一圈的点，从而造成local trajectory跳进度。

## 8. 测试与ROS2回归

Planning production-linked测试扩展为：

| Test target | Source cases | 主要范围 |
|---|---:|---|
| `test_roadnet_loader` | 19 | 路径containment、schema、hash、index、结构和恶意输入 |
| `test_planning_algorithms` | 13 | Dijkstra/A*、determinism、motion/speed |
| `test_planning_helpers` | 8 | semantic、terminal、summary、continuity、progress |

新增Planning-only launch source，覆盖canonical ready、PlanRoute、task/parking/charging PlanMission、failure emergency、invalid reload、late subscriber/QoS/republish和bounded process exit。测试不启动Control或Chassis，也不访问UDP。

本地实际状态：

- 17项offline runner全部PASS；
- 40个C++ case为`GENERATED_NOT_EXECUTED`；
- 5个runtime + 1个post-shutdown launch case为`SKIPPED_ROS2_UNAVAILABLE`。

## 9. CI最小修复

Phase 15修复container中Git safe-directory以及`offline_repository_hygiene.py`调用`git ls-files`时没有输出stderr/正确处理返回码的问题。

具体作用是让CI主job能够越过此前exit 128阻塞，真正进入后续offline、build和test步骤。

不过当前工作区没有对应的远端full CI PASS，所以`CDX-P1-007`仍不能关闭。

## 10. Finding作用与限制

本阶段对以下finding提供生产实现：

- `CDX-P1-005`：manifest path escape；
- `CDX-P2-003`：Loader结构校验；
- `CDX-P2-004`：A*正确性和确定性；
- `CDX-P2-005`：semantic route summary；
- `CDX-P2-006`：Planning回环progress。

由于新C++与ROS2测试未执行，这些状态仍是`PARTIALLY_FIXED / GENERATED_NOT_EXECUTED`，不能仅凭源码存在标记为完全FIXED。

本阶段没有修改canonical AD Package文件名、topic/service、自定义接口、Control生产逻辑、Chassis代码或正式Roadnet数据。

