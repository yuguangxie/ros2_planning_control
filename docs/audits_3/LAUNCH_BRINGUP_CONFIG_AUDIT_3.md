# Launch、Bringup 与配置审计 3

## Objective（目标）
审计 planning/control/bringup 的默认配置、topic 可配置性、sample AD Package 路径、launch arg 覆盖、参数命名和 install 后可用性。

## Status（状态）
Partial。配置和 launch 源码层面较完整，默认 `/localization/pose` 保持可配置，bringup demo 使用 package share 路径；但 ROS2 launch 和参数加载未实际运行。

## Evidence（证据）
- planning launch 使用 `FindPackageShare`：`src/low_speed_av_planning/launch/planning.launch.py:5`、`src/low_speed_av_planning/launch/planning.launch.py:12`。
- planning launch 参数覆盖：`src/low_speed_av_planning/launch/planning.launch.py:17` 到 `src/low_speed_av_planning/launch/planning.launch.py:27`。
- control launch 使用 package share：`src/low_speed_av_control/launch/control.launch.py:5`、`src/low_speed_av_control/launch/control.launch.py:12`。
- bringup demo sample path 和 config path：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:14` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:26`。
- bringup launch arg 覆盖：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:29` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:31`。
- bringup planning params 传入 package path：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:37` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:39`。
- planning config 默认 topic：`src/low_speed_av_planning/config/planning_params.yaml:24`、`src/low_speed_av_planning/config/planning_params.yaml:28` 到 `src/low_speed_av_planning/config/planning_params.yaml:32`。
- control config 默认 topic：`src/low_speed_av_control/config/control_params.yaml:12` 到 `src/low_speed_av_control/config/control_params.yaml:23`。
- bringup config 默认定位：`src/low_speed_av_bringup/config/control_params.yaml:8`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-LC-001 | P3 | Pass | `/localization/pose` 是默认值且由 YAML 配置。 |
| A3-LC-002 | P3 | Pass | planning/control 关键 topic 均在 YAML 中配置。 |
| A3-LC-003 | P3 | Pass | bringup demo 默认使用安装后的 config 和 sample AD Package，并支持 launch arg 覆盖。 |
| A3-LC-004 | P2 | Not Verified | ROS2 launch 参数命名、嵌套 YAML 到 dotted parameter 的实际加载未验证。 |
| A3-LC-005 | P3 | Partial | 部分参数是预留/experimental 选项，真实消费情况需要参数审计测试继续覆盖。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
配置默认值符合需求，降低接线错误风险。若 launch 参数或 YAML 命名在 ROS2 中未正确加载，节点可能使用空 roadnet path 或默认算法，影响 demo 可用性。

## Recommended fix（推荐修复）
- 在 ROS2 环境执行 bringup demo launch，并用 `ros2 param get` 检查关键参数。
- 将 experimental 参数在 README 或 config 注释中明确标注。
- 增加静态脚本比对 YAML key 与 C++ `declare_parameter` 名称。

## Verification method（验证方法）
- 静态读取 launch/config。
- `validate_expected_tree.py` 通过。
- 未运行 `ros2 launch` 或 `ros2 param`。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`
- `ros2 param get /planning_node roadnet.package_path`
- `ros2 param get /control_node topics.localization_pose_topic`

