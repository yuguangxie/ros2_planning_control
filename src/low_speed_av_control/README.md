# low_speed_av_control

## 模块定位
`low_speed_av_control` 是低速自动驾驶控制模块。它订阅定位、规划轨迹、车辆状态和安全状态，输出车辆跟踪控制命令。

当前默认面向 Yunle ROS2 底盘驱动输出：

```text
/yunle_chassis/control/scu_control_command
chassis_interfaces/msg/ScuControlCommand
```

默认 `output.mode: "both"`，因此同时发布规范控制合同 `/control/command` 和 Yunle SCU 命令。

## 输入
- `/localization/pose`：默认定位输入，类型 `geometry_msgs/msg/PoseStamped`。
- `/planning/trajectory`：规划轨迹；只允许配置白名单内的状态，默认仅 `ok`。
- `/vehicle/state`：可通过 `vehicle_state.required` 配置为必需；一旦收到，自治许可、人工制动、故障和超时始终参与门控。
- `/safety/status`：安全状态，`level >= 2` 或 `state=estop/emergency_stop/failure` 会触发停车。

所有话题名均在 `config/control_params.yaml` 中配置。

## 输出
- `/yunle_chassis/control/scu_control_command`：底盘控制命令，类型 `chassis_interfaces/msg/ScuControlCommand`。
- `/control/command`：内部调试命令，类型 `low_speed_av_interfaces/msg/ControlCommand`。
- `/control/status`：控制模块状态。

## 安全状态机与输入合同

控制入口先执行 ROS-independent 安全判定，再决定是否调用控制器。状态为 `WAIT_INPUTS`、`READY`、`ACTIVE`、`CONTROLLED_STOP`、`ESTOP_LATCHED`。原因优先级依次为：安全急停或显式 emergency trajectory、车辆故障/自治未许可/人工制动、定位失效、轨迹失效、正常跟踪。

轨迹必须满足：`trajectory_id` 与 `source_package_id` 非空、`status` 在白名单内、`emergency_stop=false`、点非空、所有数值有限、`s_m` 在容差内单调、gear 合法。failure trajectory 即使带点或非零目标速度也直接停车，不进入 Pure Pursuit、Stanley、LQR 或 MPC sampler；两种车辆模型得到相同的零速、零转角、制动输出。

本地 watchdog 使用消息接收时的 `std::chrono::steady_clock`，不依赖 header stamp，也不会因仿真时间为零或暂停而失效。header stamp 仅保留为消息来源时间，不作为本阶段本地超时的唯一依据。

当 `safety.estop_latched=true` 时，普通 `ok/standby` 心跳不能清除急停。必须调用：

```bash
ros2 service call /low_speed_av_control/clear_estop std_srvs/srv/Trigger "{}"
```

清除要求安全请求已撤销、定位和轨迹有效且新鲜、存在有效且新鲜的 VehicleState、车速不超过阈值、无故障、未踩制动且自治已许可。成功后先进入 `READY`，下一控制周期重新检查，不能直接跳变为运动。

## 控制器
控制器由 `ControllerFactory` 选择：

- `pure_pursuit`：预瞄点跟踪，输出期望曲率。
- `stanley`：横向误差和航向误差反馈，输出期望曲率。
- `lqr`：离散运动学自行车模型 LQR，使用 Riccati 迭代求解反馈增益，并叠加曲率前馈。
- `mpc_sampler`：确定性采样控制器，不依赖重型求解器，仍建议作为实验/对比算法使用。

控制器统一输出内部字段 `desired_curvature_1pm`。车辆模型再将曲率转换为前/后轮物理转角。

## LQR 控制器
LQR 使用状态：

```text
x = [e_y, e_psi]^T
A = [[1, v*dt],
     [0, 1]]
B = [[0],
     [v*dt/wheel_base]]
```

误差约定：

```text
e_y = -sin(yaw_ref)*(x_vehicle - x_ref) + cos(yaw_ref)*(y_vehicle - y_ref)
e_psi = normalize_angle(yaw_vehicle - yaw_ref)
```

输出：

```text
delta_ff = atan(wheel_base * kappa_ref)
delta_fb = -K * [e_y, e_psi]^T
delta_cmd = delta_ff + delta_fb
desired_curvature_1pm = tan(delta_cmd) / wheel_base
```

LQR 可通过 `lqr.*` 参数调节 Q/R、迭代次数、收敛阈值、最小建模速度、预瞄时间和最大转角。

## 车辆模型
- `front_ackermann`：`kappa = tan(delta_front) / wheel_base`，后轮转角为 0。
- `dual_ackermann`：`kappa = (tan(delta_front) - tan(delta_rear)) / wheel_base`，后轮按 `rear_steer_ratio` 反相转向。

## Yunle SCU 映射
`ScuCommandMapper` 将内部 SI 单位命令转换为 `chassis_interfaces/msg/ScuControlCommand`：

- 速度：`m/s -> km/h`，使用绝对值。
- 转角：`rad -> deg`。
- 挡位：drive -> 1，neutral -> 2，reverse -> 3。
- 未知挡位不会发布非法 shift，而是发布配置挡位的刹车停车命令。
- safety stop、estop、timeout、empty trajectory、NaN/Inf guard 都会映射为刹车停车命令。

详见 `docs/YUNLE_SCU_COMMAND_OUTPUT.md`。

## 无 ROS2 离线检查
当前 Windows Codex 环境可能没有 ROS2。可运行：

```powershell
python scripts\offline_algorithm_smoke.py
python scripts\offline_remaining_fixes_smoke.py
python scripts\offline_scu_lqr_smoke.py
```

如果默认 `python` 不可用，可使用已知可用解释器运行。不要在无 ROS2 环境声称 `colcon build` 或 `ros2 launch` 成功。

## ROS2 环境验证
在真实 ROS2 环境中再运行：

```bash
colcon build --packages-select low_speed_av_interfaces low_speed_av_control
colcon test --packages-select low_speed_av_control
ros2 launch low_speed_av_control control.launch.py params:=/path/to/control_params.yaml
ros2 topic echo /yunle_chassis/control/scu_control_command
```
