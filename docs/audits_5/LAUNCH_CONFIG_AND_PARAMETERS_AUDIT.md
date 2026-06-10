# Launch Config And Parameters Audit

## Objective

审计 launch、YAML 参数和 package 安装规则是否与代码声明、topic 命名和使用文档一致。

## Scope

- `src/*/config/*.yaml`
- `src/*/launch/*.py`
- `src/*/CMakeLists.txt`
- `src/*/package.xml`

## Status

Partial。静态结构可读，ROS2 launch 未验证。

## Evidence

- planning 参数包含新增 `planning` key：YAML 解析输出 `dict_keys(['roadnet', 'topics', 'global_planner', 'motion_planner', 'speed_planner', 'planning'])`。
- simulation YAML 包含两个节点 key：`roadnet_visualization_node` 和 `sim_localization_pose_publisher`。
- bringup launch 注入 `roadnet.package_path`：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py`。
- simulation launch 参数：`src/low_speed_av_simulation/launch/simulation_visualization.launch.py:43` 到 `src/low_speed_av_simulation/launch/simulation_visualization.launch.py:48`。
- simulation launch 可包含 planning/control：`src/low_speed_av_simulation/launch/simulation_visualization.launch.py:49` 到 `src/low_speed_av_simulation/launch/simulation_visualization.launch.py:55`。
- CMake install simulation config/launch/rviz：`src/low_speed_av_simulation/CMakeLists.txt:45`。

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-LC-001 | P3 | Pass | 新 planning YAML 参数与代码声明一致。 | current-pose 配置可加载。 | 无。 | `ros2 param get`。 |
| AUD5-LC-002 | P2 | Partial | simulation launch default roadnet 指向 bringup sample package，而目标文档多使用外部 `roadnet_ad_package_20260610T012525Z`。 | 用户忘记传参时看到的是 sample，不是当前目标包。 | 文档已强调传 `roadnet_package_path`；后续可在 repo 安装目标包或增加提示。 | launch 后检查 loaded package id。 |
| AUD5-LC-003 | P1 | Not Verified | launch include `low_speed_av_bringup` 和 RViz config 未实际运行。 | 可能存在 launch substitution 或 package share 问题。 | ROS2 环境执行 launch。 | `ros2 launch low_speed_av_simulation ...`。 |
| AUD5-LC-004 | P2 | Partial | 部分旧 docs 仍含过期 line refs/行为描述。 | 人工测试时可能读到旧结论。 | 后续文档 cleanup。 | `rg "当前 planning node 没有创建 subscription" docs`。 |

## ROS2 Commands Run Or Skipped

Run:

- YAML parse via `uv run --with pyyaml ...`
- CMake/package static scan

SKIPPED_ROS2_UNAVAILABLE:

- `ros2 launch low_speed_av_simulation simulation_visualization.launch.py ...`
- `ros2 param get /low_speed_av_planning planning.use_current_pose_as_start`

## Remaining Uncertainty

真实 launch 参数覆盖、install share 路径和 RViz config 兼容性未验证。

