# SCU Output and LQR Upgrade Report

## Goal
将控制模块的底盘输出改为 Yunle ROS2 底盘 `chassis_interfaces/msg/ScuControlCommand`，并将 LQR 升级为基于离散 Riccati 迭代的轨迹跟踪控制器。

## Changed files
- `src/low_speed_av_control/include/low_speed_av_control/control_types.hpp`
- `src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp`
- `src/low_speed_av_control/include/low_speed_av_control/control_node.hpp`
- `src/low_speed_av_control/include/low_speed_av_control/scu_command_mapper.hpp`
- `src/low_speed_av_control/src/scu_command_mapper.cpp`
- `src/low_speed_av_control/src/control_node.cpp`
- `src/low_speed_av_control/src/lqr_controller.cpp`
- `src/low_speed_av_control/src/pure_pursuit_controller.cpp`
- `src/low_speed_av_control/src/stanley_controller.cpp`
- `src/low_speed_av_control/src/mpc_sampler_controller.cpp`
- `src/low_speed_av_control/src/command_limiter.cpp`
- `src/low_speed_av_control/src/command_smoother.cpp`
- `src/low_speed_av_control/CMakeLists.txt`
- `src/low_speed_av_control/package.xml`
- `src/low_speed_av_control/config/control_params.yaml`
- `src/low_speed_av_bringup/config/control_params.yaml`
- `src/low_speed_av_control/README.md`
- `scripts/offline_scu_lqr_smoke.py`
- `scripts/offline_remaining_fixes_smoke.py`
- `scripts/validate_expected_tree.py`
- `scripts/check_ros2_env.ps1`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `docs/LQR_CONTROLLER_DESIGN.md`
- `docs/ROS2_INTEGRATION_TEST_PLAN.md`

## ROS2 dependency changes
`low_speed_av_control` 新增：

```cmake
find_package(builtin_interfaces REQUIRED)
find_package(chassis_interfaces REQUIRED)
```

并在 `package.xml` 中新增：

```xml
<depend>builtin_interfaces</depend>
<depend>chassis_interfaces</depend>
```

## SCU output
- Topic: `/yunle_chassis/control/scu_control_command`
- Type: `chassis_interfaces/msg/ScuControlCommand`
- C++ include: `chassis_interfaces/msg/scu_control_command.hpp`

## Mapping table
| Internal command | ScuControlCommand |
|---|---|
| `gear=drive` | `scu_shift_level_request=1` |
| `gear=neutral` | `scu_shift_level_request=2` |
| `gear=reverse` | `scu_shift_level_request=3` |
| unknown gear | safe brake stop, never invalid shift |
| `speed_mps` | `abs(speed_mps) * 3.6` km/h |
| `front_steering_angle_rad` | degrees times `front_steer_sign` |
| `rear_steering_angle_rad` | degrees times `rear_steer_sign` |
| `brake` or `emergency_stop` | brake stop command |

## Safety stop mapping
安全停车输出 `stop_shift_level`、0 转角、0 km/h、`scu_brake_enable=true`，并强制灯光为 0、valid flags 为 false。

## LQR model equations
```text
x = [e_y, e_psi]^T
A = [[1, v*dt],
     [0, 1]]
B = [[0],
     [v*dt/wheel_base]]
P_next = A^T P A - A^T P B (R + B^T P B)^-1 B^T P A + Q
K = (R + B^T P B)^-1 B^T P A
delta_cmd = atan(wheel_base*kappa_ref) - K*[e_y, e_psi]^T
```

## LQR config
```yaml
controller:
  algorithm: "lqr"
lqr:
  q_lateral_error: 3.0
  q_heading_error: 2.0
  r_steering: 1.0
  max_iterations: 80
  convergence_eps: 1.0e-6
  min_speed_mps: 0.2
  preview_time_s: 0.2
  use_curvature_feedforward: true
  max_steering_angle_rad: 0.52
```

## Tests run
- `python scripts\validate_expected_tree.py`：失败，当前 Windows 默认 `python` 不可用，stdout 为空。
- `py scripts\validate_expected_tree.py`：失败，`py` 命令不存在。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_remaining_fixes_smoke.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_scu_lqr_smoke.py`：通过。
- `powershell -ExecutionPolicy Bypass -File scripts\check_ros2_env.ps1`：正确输出 `SKIPPED_ROS2_UNAVAILABLE`。
- `package.xml` XML 解析检查：通过。
- 静态检索确认未引入 `catkin`、`roscpp`、`scu_drive_mode_request`。

## SKIPPED_ROS2_UNAVAILABLE
当前 Windows Codex 环境未检测到 ROS2，不运行也不声称以下命令成功：

- `colcon build`
- `colcon test`
- `ros2 launch`
- `ros2 topic echo /yunle_chassis/control/scu_control_command`

## Remaining integration steps
1. 在真实 ROS2 环境安装或提供 `chassis_interfaces`。
2. 运行 `colcon build --packages-select low_speed_av_interfaces low_speed_av_control`。
3. 启动 control node，确认 SCU topic 类型和字段。
4. 发布 pose、trajectory、safety status，验证正常跟踪和 estop 停车。
