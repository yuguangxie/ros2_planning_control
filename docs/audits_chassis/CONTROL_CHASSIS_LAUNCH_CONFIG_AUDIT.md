# Control 与 Chassis Launch/Config 审计

## Objective

审计 control、bringup 和 chassis_driver 的默认参数、launch 启动方式、命名空间、topic remap 与构建依赖，判断是否能让 control 与 chassis_driver 在 ROS2 Humble workspace 中对接。

## Scope

- `src/low_speed_av_control/config/control_params.yaml`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `src/low_speed_av_bringup/launch/planning_control_demo.launch.py`
- `src/low_speed_av_control/launch/control.launch.py`
- `src/yunle_chassis/chassis_driver/config/chassis_driver.yaml`
- `src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py`
- package.xml 和 CMakeLists.txt

## Status: Partial

默认 topic/config 匹配，构建依赖静态合理；但现有 `low_speed_av_bringup` demo launch 不启动 chassis_driver，需要人工另起 driver。

## Evidence

- control package 依赖 `chassis_interfaces`：`src/low_speed_av_control/package.xml:14`。
- control CMake `find_package(chassis_interfaces REQUIRED)`：`src/low_speed_av_control/CMakeLists.txt:13`。
- control target dependency 包含 `chassis_interfaces`：`src/low_speed_av_control/CMakeLists.txt:35` 到 `src/low_speed_av_control/CMakeLists.txt:41`。
- chassis_interfaces 使用 `rosidl_generate_interfaces`：`src/yunle_chassis/chassis_interfaces/CMakeLists.txt:27` 到 `src/yunle_chassis/chassis_interfaces/CMakeLists.txt:30`。
- chassis_driver package 依赖 `chassis_interfaces`：`src/yunle_chassis/chassis_driver/package.xml:14`。
- chassis_driver CMake 依赖 `rclcpp` 和 `chassis_interfaces`：`src/yunle_chassis/chassis_driver/CMakeLists.txt:11` 到 `src/yunle_chassis/chassis_driver/CMakeLists.txt:15`。
- control YAML SCU topic：`src/low_speed_av_control/config/control_params.yaml:19`。
- bringup YAML SCU topic：`src/low_speed_av_bringup/config/control_params.yaml:15`。
- chassis_driver YAML topic prefix：`src/yunle_chassis/chassis_driver/config/chassis_driver.yaml:11`。
- chassis_driver launch 启动 `chassis_driver_node`：`src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py:13` 到 `src/yunle_chassis/chassis_driver/launch/chassis_driver.launch.py:20`。
- planning/control demo launch 只启动 planning 和 control：`src/low_speed_av_bringup/launch/planning_control_demo.launch.py:32` 到 `src/low_speed_av_bringup/launch/planning_control_demo.launch.py:48`。
- bringup package 只有 `low_speed_av_planning` 和 `low_speed_av_control` exec depend：`src/low_speed_av_bringup/package.xml:10` 到 `src/low_speed_av_bringup/package.xml:13`。

## Findings

| ID | Severity | Finding |
|---|---|---|
| CHAS-LAUNCH-001 | P2 | `planning_control_demo.launch.py` 不启动 `chassis_driver_node`，因此 control 的 SCU topic 需要另一个 launch 才会有 driver subscriber。 |
| CHAS-LAUNCH-002 | P2 | `low_speed_av_bringup/package.xml` 未声明 `chassis_driver` exec dependency；如果未来把 chassis driver 纳入 bringup，需要同步增加依赖。 |
| CHAS-LAUNCH-003 | P2 | control 的 `scu.max_target_speed_kmh=5.0` 与 driver 的 `scu_control_max_target_speed_kmh=15.0` 不相等；当前安全偏保守，但需在 bench 配置表中明确。 |
| CHAS-LAUNCH-004 | P3 | `src/yunle_chassis` 的部分中文注释在当前 Windows 输出中显示为乱码，可能是编码或终端显示问题；源码逻辑不受影响，但文档可读性需检查。 |

## Impact

- 对集成：单独运行 `planning_control_demo.launch.py` 只能产生 control publisher，不能保证底盘 driver 已经订阅。
- 对 bench 验证：必须使用两个 launch 或人工启动 driver。
- 对构建：静态依赖为 ROS2/ament，未发现 catkin/roscpp 运行依赖。

## Recommended Fix

本轮只审计。后续可以新增一个可选参数的 bringup launch：

- `launch_chassis_driver:=true/false`
- `chassis_driver_params:=...`
- bench-only 默认 `false`，避免误连真实底盘。

## Verification Method

```bash
ros2 launch low_speed_av_bringup planning_control_demo.launch.py
ros2 launch chassis_driver chassis_driver.launch.py
ros2 topic info /yunle_chassis/control/scu_control_command
```

期望：

- `Publisher count: 1`
- `Subscription count: 1`
- Type 为 `chassis_interfaces/msg/ScuControlCommand`

## ROS2 Commands Skipped Or Run

- `SKIPPED_ROS2_UNAVAILABLE`: 当前未运行 ROS2 launch/topic 命令。
