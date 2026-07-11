# 测试、文档与工程化审计

## 本轮实际执行结果

使用 `C:\Program Files\FreeCAD 1.2\bin\python.exe` 执行。系统 `python/python3` 是 Windows Store placeholder，`pytest` 未安装，ROS2/colcon 不存在。

| 检查 | 结果 |
|---|---|
| `validate_expected_tree.py` | PASS |
| `validate_sample_ad_package.py`（sample） | PASS：3 nodes / 2 edges / 6 waypoints |
| `validate_sample_ad_package.py`（正式包 `_1`） | PASS：20 / 26 / 737 |
| `validate_sample_ad_package.py`（正式包 `_2`） | PASS：16 / 22 / 496 |
| `offline_algorithm_smoke.py`（默认 sample） | PASS |
| `offline_remaining_fixes_smoke.py` | PASS |
| `offline_reverse_policy_smoke.py` | PASS |
| `offline_scu_lqr_smoke.py` | PASS |
| `offline_semantic_goal_followup_smoke.py` | PASS |
| `offline_sim_localization_follow_smoke.py` | PASS |
| `offline_trajectory_continuity_smoke.py` | PASS，覆盖两套正式包 |
| `offline_runtime_followup_smoke.py` 默认命令 | FAIL：默认目录不存在 |
| 同脚本显式传 `_2` | PASS |
| `offline_simulation_smoke.py` 默认命令 | FAIL：默认目录不存在 |
| 同脚本显式传 `_2` | PASS |
| `offline_algorithm_smoke.py` 显式传正式包 `_2` | FAIL：parking_points 为空导致 IndexError；该脚本实际只适配 sample fixture |
| 全部 74 个 JSON | PowerShell `ConvertFrom-Json` 解析通过 |
| 447 个文本类文件 UTF-8 严格解码 | 0 个解码错误 |
| Markdown 本地链接 | 35 个本地链接，0 个失效 |
| `scripts/check_ros2_env.ps1` | 正确输出 `SKIPPED_ROS2_UNAVAILABLE` |
| `colcon build/test`、ROS2 launch/topic/service | `SKIPPED_ROS2_UNAVAILABLE` |

## 测试覆盖的真实含义

现有 smoke 的价值是：验证数据包结构、checksum、典型 route、几何连续性和若干数学公式，且无需 ROS2，适合开发早期快速回归。

其局限同样明显：

- 多数脚本重新用 Python 实现 Dijkstra、trajectory stitch、Ackermann、LQR 或 SCU mapping，不直接调用 C++。
- `offline_remaining_fixes_smoke.py` 包含大量源码 token 断言，只能证明字符串存在，不能证明路径执行正确。
- 唯一注册的 pytest 只是 subprocess 包装 `offline_trajectory_continuity_smoke.py`。
- 没有针对异常输入、并发、timeout、QoS、lifecycle、UDP、DBC bit packing 的 C++/ROS2 测试。
- 本轮直接运行 pytest 文件并不等于执行 test function；当前 Python 环境也没有 pytest。

## 推荐测试金字塔

### 第一层：纯 C++ 单元测试

为下列生产类建立 gtest：

- RoadnetLoader：合法包、failed validation、hash mismatch、path escape、重复 ID、负 cost、index 两种 end 形式。
- Dijkstra/A*：不可达、blocked、reverse、等价代价确定性、A* 与 Dijkstra 最优结果一致。
- Trajectory builder：edge boundary、semantic terminal、回环、同 edge 前后目标、route s 单调。
- Controllers：forward/reverse、空轨迹、零速轨迹、NaN、回环最近点、方向切换。
- Limiter/Smoother：前后轮 rate、accel/decel、实际 dt、急停 bypass、参数非法。
- ScuCommandMapper：四种 gear、单位、overrange、emergency、brake、NaN。
- DBC/codec：每个 signal 边界值、符号、DLC、13-byte round trip、malformed payload。

### 第二层：ROS2 组件/launch 测试

- Planning service 成功/失败后核对 route、trajectory、status。
- failure trajectory 到 Control 后核对 internal 与 SCU brake。
- localization/trajectory/safety timeout 与恢复。
- algorithm switch、roadnet reload、late subscriber QoS。
- Chassis command watchdog、driver startup/shutdown stop。

### 第三层：SIL/HIL

- 让 ControlCommand 驱动运动学车辆模型，不再直接回放 planning path。
- 记录 rosbag、CAN capture、延迟、抖动、跟踪误差、停车距离和故障注入。
- bench-only/wheels-off 后再评估低速封闭场地。

## 配置审计

代码已声明但完全未读取的 Planning 参数：

- `planning.semantic_goal_use_edge_projection`
- `planning.semantic_goal_allow_reverse_local_segment`
- `planning.reverse.require_reverse_confirmation`
- `planning.reverse.prefer_forward_route_when_reverse_disabled`

YAML 存在但节点未声明/未实现的常见参数：

- Planning：`roadnet.allow_validation_warning`、`roadnet.supported_schema_versions`、`topics.localization_pose_type`、`global_planner.max_edge_curvature_abs`、`motion_planner.horizon_time_s`、`nearest_search_radius_m`、`speed_planner.min_speed_mps`。
- Control：`topics.localization_pose_type`、`controller.hold_last_command_s`、车辆外形参数、`command_smoother.max_accel_mps2/max_decel_mps2/emergency_decel_mps2/stop_enable_mode`。

这类“看起来可配、实际无效”的参数比缺少参数更危险。建议建立 config schema，并在测试中比较 YAML leaf keys 与节点 declare/get 列表。

## 文档审计

仓库有约 200 份 Markdown、约 1.5 万行，覆盖面很广，但信息架构负担较重：

- 77 份历史 audit 文档、41 份 report 与当前说明同时存在。
- 旧审计中的“未实现 checksum”等结论已过时，但没有统一 superseded 标记。
- `docs/07_config_launch_runtime.md` 列出的 `planning_params_file`、`control_params_file`、`use_sim_time` 与当前 launch 不一致。
- 多份 `UBUNTU_ROS2_REVALIDATION_*` 是操作步骤，却使用“构建通过”等期望式文字，容易被误认为已有结果。
- README、模块 README、设计文档、操作文档和审计报告重复描述同一 topic/config，漂移概率高。

建议保留五类 canonical 文档：架构、接口、配置、操作、验证记录。历史 audits/reports 移到 `docs/archive/<date>/` 并加 metadata：`status: archived`、`superseded_by`、`tested_commit`。

## 模板与样例数据

sample AD Package 在 `templates/` 和 bringup 中有 23 个完全相同的文件，当前尚未漂移；但 sample config 和 offline validation template 已明显落后于运行目录版本。模板不是单一真源，也没有同步测试。

建议只保留一个 canonical 模板源，通过脚本生成/复制 bringup sample，并在 CI 比较 hash。正式 Roadnet 包作为 immutable fixture，应记录来源、许可、数据版本和测试用途。

## 工程治理

当前已有 `.gitignore`、CMake warning flags、package.xml 和较多中文说明，这是良好起点。仍缺少：

- GitHub/GitLab CI；
- clang-format/clang-tidy/cppcheck/sanitizer；
- 仓库根 LICENSE（虽然 package 声明 Apache-2.0）；
- CONTRIBUTING、SECURITY、CODEOWNERS、release checklist；
- 覆盖率、测试报告、制品归档；
- 参数 schema、ROS distro/依赖 lock、容器化开发环境；
- 真实底盘安全 case 和 traceable requirement matrix。

## Phase 14 状态补充（2026-07-11）

本轮在明确授权的 Phase 13 gate override 下继续实施，阶段状态为 `IMPLEMENTED_WITH_ACCEPTED_PHASE_13_GAP`。Planning、Control 和当前 Chassis production core 已注册直接链接 production target 的 gtest；ROS2 launch test、跨平台 offline runner、template consistency check 和 GitHub Actions workflow 已配置。

- `CDX-P1-006`：`PARTIALLY_FIXED`。测试底座和源码已建立，但当前 Windows 环境未编译执行 C++。
- `CDX-P1-007`：`CONFIGURED_NOT_EXECUTED`。CI 已配置，本地无 ROS2，且尚无当前提交的远端 CI 成功证据。
- `CDX-P3-001`：`FIXED`。runtime/simulation smoke 默认选择现存正式包 `_2`，并有入口回归检查。
- `CDX-P3-002`：`FIXED`。空 `parking_points` 返回可解释的 `SKIPPED_EMPTY_PARKING_POINTS`，不再抛出未处理 `IndexError`。
- `CDX-P3-003`：`FIXED`。`scripts` 为离线验证 canonical source，配置与 sample 的复制关系由自动检查约束。
- `CDX-P0-002`：`OPEN / ACCEPTED_PHASE_14_GAP`。本轮 Chassis 测试只覆盖现有 DBC、codec 和 frame 构造路径；缺失的独立 scheduler/watchdog、startup/timeout/shutdown stop 与 diagnostics 未被伪造为已实现或已通过。

测试层级、执行状态和复现方法见 `docs/PHASE14_TEST_MATRIX.md` 与 `reports/phase_14_report.md`。
