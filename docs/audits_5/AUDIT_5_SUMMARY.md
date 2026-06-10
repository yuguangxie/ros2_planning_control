# Audit 5 Summary

## Objective

对当前 ROS2 低速自动驾驶工程的所有功能模块进行第 5 轮完整静态审计，并生成可人工执行的 ROS2 验证流程。审计覆盖 interfaces、planning、current-pose start、control、SCU safety、simulation visualization、bringup/config、AD Package、offline tests 和端到端数据流。

## Scope

- `src/low_speed_av_interfaces`
- `src/low_speed_av_planning`
- `src/low_speed_av_control`
- `src/low_speed_av_simulation`
- `src/low_speed_av_bringup`
- `scripts/`
- `docs/`
- `reports/`
- `roadnet_ad_package_20260610T012525Z`

## Status

Partial。

源码和离线脚本层面，多数功能路径具备实现和证据；但当前 Windows Codex 环境未提供 `colcon`/`ros2`，因此 ROS2 build、launch、service、topic、RViz 和真实 chassis driver 行为均为 Not Verified。

## Evidence

- 五个 ROS2 包存在：`src/low_speed_av_interfaces`、`src/low_speed_av_planning`、`src/low_speed_av_control`、`src/low_speed_av_simulation`、`src/low_speed_av_bringup`。
- Planning 服务和 publishers：`src/low_speed_av_planning/src/planning_node.cpp:108` 到 `src/low_speed_av_planning/src/planning_node.cpp:136`。
- Planning 当前定位订阅：`src/low_speed_av_planning/src/planning_node.cpp:116`。
- Control 输入、SCU 输出和状态 publisher：`src/low_speed_av_control/src/control_node.cpp:83` 到 `src/low_speed_av_control/src/control_node.cpp:101`。
- SCU mapper stop/sanitize 逻辑：`src/low_speed_av_control/src/scu_command_mapper.cpp:89` 到 `src/low_speed_av_control/src/scu_command_mapper.cpp:141`。
- Simulation CMake 结构：`src/low_speed_av_simulation/CMakeLists.txt:8` 到 `src/low_speed_av_simulation/CMakeLists.txt:45`。
- Simulation launch：`src/low_speed_av_simulation/launch/simulation_visualization.launch.py:43` 到 `src/low_speed_av_simulation/launch/simulation_visualization.launch.py:92`。
- AD Package 校验结果：`AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-SUM-001 | P1 | Not Verified | ROS2 build/test/launch 未在真实 ROS2 环境执行。 | 可能存在 CMake、链接、参数或 launch 运行时问题。 | 在 Ubuntu 22.04 + ROS2 环境执行完整人工验证。 | `colcon build --symlink-install`、`colcon test`、launch smoke。 |
| AUD5-SUM-002 | P2 | Partial | 当前 pose 起点匹配是 waypoint 最近邻 + edge progress 启发式，不是连续投影局部规划。 | 起点在边中后段时可能从 `to_node` 起步，跳过当前边剩余部分。 | 后续实现 edge projection 和 route prefix/crop。 | 构造中段 pose 和多方向 edge 测试。 |
| AUD5-SUM-003 | P2 | Partial | RViz waypoint 基础显示用单条 LINE_STRIP 连接全部 waypoint，可能跨 edge 产生视觉连线。 | 可视化可能误导人工判断。 | 按 edge 分 Marker 或使用 LINE_LIST/多个 namespace。 | RViz 检查跨边连接。 |
| AUD5-SUM-004 | P1 | Not Verified | 新增 `low_speed_av_simulation` 包未经过 colcon 编译验证。 | 可能存在链接 `low_speed_av_planning` 或 RViz config 兼容问题。 | 在 ROS2 环境编译并启动 simulation launch。 | `colcon build --packages-select low_speed_av_simulation`。 |
| AUD5-SUM-005 | P2 | Partial | 部分旧文档仍描述 planning 未订阅定位，与当前代码变化不一致。 | 使用者可能被旧文档误导。 | 后续统一更新旧走读文档。 | `rg "planning node 没有创建 subscription" docs`。 |

## Module Status

| Module | Static status | Runtime ROS2 status |
|---|---|---|
| interfaces | Pass | Not Verified |
| roadnet AD Package | Pass | Not Verified in ROS2 node |
| planning | Partial | Not Verified |
| current-pose start | Partial | Not Verified |
| control | Pass by static/offline | Not Verified |
| SCU output/safety | Pass by static/offline | Not Verified with chassis driver |
| simulation visualization | Partial | Not Verified |
| bringup/launch/config | Partial | Not Verified |
| offline scripts | Pass | N/A |

## ROS2 Commands Run Or Skipped

Run:

- `git status --short`
- `git diff --stat`
- `rg -n "..."`
- `uv run python scripts\validate_expected_tree.py`
- `uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z`
- `uv run --with pyyaml python scripts\offline_algorithm_smoke.py src\low_speed_av_bringup\sample_ad_package`
- `uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py`
- `uv run --with pyyaml python scripts\offline_scu_lqr_smoke.py`
- `uv run --with pyyaml python scripts\offline_simulation_smoke.py roadnet_ad_package_20260610T012525Z`

SKIPPED_ROS2_UNAVAILABLE:

- `rosdep install --from-paths src --ignore-src -r -y`
- `colcon build --symlink-install`
- `colcon test`
- `colcon test-result --verbose`
- `ros2 launch low_speed_av_simulation simulation_visualization.launch.py ...`
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 topic echo ...`
- `ros2 service call ...`

## Remaining Uncertainty

- 未验证真实 ROS2 编译和链接。
- 未验证 RViz marker 视觉效果。
- 未验证 `chassis_interfaces/msg/ScuControlCommand` 在目标 ROS2 workspace 的真实字段。
- 未验证真实底盘驱动是否接收并执行 SCU 命令。
- 未验证 current-pose matcher 在复杂路网、反向边、交叉路口、定位噪声下的行为。

