# ROS2 集成就绪度审计 3

## Objective（目标）
审计当前项目进入真实 ROS2 环境后的 build/test/launch/service/topic 检查计划、脚本行为和剩余运行时风险。

## Status（状态）
Partial。项目已有 `check_ros2_env.ps1` 和 ROS2 集成测试计划，跳过行为清晰；但所有 ROS2 命令尚未实际执行。

## Evidence（证据）
- ROS2 环境检查脚本在 colcon 缺失时输出 skipped：`scripts/check_ros2_env.ps1:15`。
- ROS2 环境检查脚本在 ros2 缺失时输出 skipped：`scripts/check_ros2_env.ps1:21`。
- 脚本明确不执行 ROS2 命令：`scripts/check_ros2_env.ps1:38`。
- 集成计划说明无 ROS2 时不宣称成功：`docs/ROS2_INTEGRATION_TEST_PLAN.md:4`。
- 集成计划包含 build/test/launch/service/topic 检查：`docs/ROS2_INTEGRATION_TEST_PLAN.md:19` 到 `docs/ROS2_INTEGRATION_TEST_PLAN.md:59`。
- 当前执行 `powershell -ExecutionPolicy Bypass -File scripts\check_ros2_env.ps1` 输出 `SKIPPED_ROS2_UNAVAILABLE`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-RI-001 | P1 | Not Verified | 真实 ROS2 build/test/launch/service/topic 全部未验证。 |
| A3-RI-002 | P3 | Pass | 无 ROS2 环境下脚本不会误报成功，会打印 `SKIPPED_ROS2_UNAVAILABLE`。 |
| A3-RI-003 | P2 | Partial | 集成计划足够启动人工验证，但还没有自动化 CI 或 Docker 环境。 |
| A3-RI-004 | P2 | Not Verified | ROS2 QoS、参数加载、service request/response、topic message 字段兼容性未验证。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
缺少 ROS2 运行时验证是当前最大集成风险。即使源码逻辑审计通过，实车或仿真前仍必须确认节点能构建、启动、收发消息并按安全策略输出命令。

## Recommended fix（推荐修复）
- 在真实 ROS2 环境中严格按 `docs/ROS2_INTEGRATION_TEST_PLAN.md` 执行。
- 增加 CI 工作流或 Docker Compose 验证 `colcon build/test`。
- 将 service/topic 验证脚本化，形成可重复集成 smoke。

## Verification method（验证方法）
- 已运行 `scripts/check_ros2_env.ps1` 并确认 skipped。
- 未运行任何 ROS2 命令。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon build --symlink-install`
- `colcon test`
- `colcon test-result --verbose`
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 service call /low_speed_av_planning/plan_route low_speed_av_interfaces/srv/PlanRoute ...`
- `ros2 topic echo /planning/trajectory`
- `ros2 topic pub /localization/pose geometry_msgs/msg/PoseStamped ...`
- `ros2 topic pub /safety/status low_speed_av_interfaces/msg/ModuleStatus ...`
- `ros2 topic echo /control/command`
