# 控制模块代码走读

包：`src/low_speed_av_control`

职责：订阅定位、规划轨迹、车辆状态和安全状态，选择控制器和车辆模型，输出内部控制命令和 Yunle SCU 底盘命令。

## 1. 主节点

主节点类：`low_speed_av_control::ControlNode`

主文件：

- `src/low_speed_av_control/src/control_node.cpp`
- `src/low_speed_av_control/include/low_speed_av_control/control_node.hpp`

节点名：`low_speed_av_control`

证据：`src/low_speed_av_control/src/control_node.cpp:14`。

## 2. 参数声明

`ControlNode` 在构造函数声明参数：

| 行号 | 参数组 | 作用 |
|---:|---|---|
| 17-23 | `topics.*` | 输入/输出 topic |
| 24 | `output.mode` | `internal`、`scu_control_command`、`both` |
| 25-27 | `safety.*` | Estop 是否 latch 以及 clear 条件 |
| 28-31 | `controller.*` | 控制器、频率、超时 |
| 32-41 | `vehicle.*` | 车辆模型和约束 |
| 42-47 | `pure_pursuit`、`stanley` | 控制器参数 |
| 48-56 | `lqr.*` | LQR 参数 |
| 57-65 | `mpc_sampler.*` | MPC sampler 参数 |
| 66-67 | `command_smoother.*` | 平滑参数 |
| 68-79 | `scu.*` | Yunle SCU 输出配置 |

bringup 默认配置在 `src/low_speed_av_bringup/config/control_params.yaml`，其中 `controller.algorithm` 为 `lqr`，`output.mode` 为 `scu_control_command`。

## 3. Subscribers

| Subscription | 默认 topic | 类型 | 回调 | 证据 |
|---|---|---|---|---|
| 定位 | `/localization/pose` | `geometry_msgs/msg/PoseStamped` | `on_pose` | `control_node.cpp:83` |
| 轨迹 | `/planning/trajectory` | `low_speed_av_interfaces/msg/Trajectory` | `on_trajectory` | `control_node.cpp:86` |
| 车辆状态 | `/vehicle/state` | `low_speed_av_interfaces/msg/VehicleState` | `on_vehicle_state` | `control_node.cpp:89` |
| 安全状态 | `/safety/status` | `low_speed_av_interfaces/msg/ModuleStatus` | `on_safety_status` | `control_node.cpp:92` |

## 4. Publishers

| Publisher | 默认 topic | 类型 | 说明 | 证据 |
|---|---|---|---|---|
| internal command | `/control/command` | `low_speed_av_interfaces/msg/ControlCommand` | `output.mode=internal` 或 `both` 时发布 | `control_node.cpp:95` |
| SCU command | `/yunle_chassis/control/scu_control_command` | `chassis_interfaces/msg/ScuControlCommand` | 默认底盘输出 | `control_node.cpp:97` |
| status | `/control/status` | `low_speed_av_interfaces/msg/ModuleStatus` | 控制状态 | `control_node.cpp:99` |

## 5. Service server

`/low_speed_av_control/set_controller_algorithm`

类型：`low_speed_av_interfaces/srv/SetControllerAlgorithm`

字段：

```text
string controller_algorithm
string vehicle_model
---
bool success
string message
```

证据：

- `src/low_speed_av_interfaces/srv/SetControllerAlgorithm.srv:2`。
- `src/low_speed_av_control/src/control_node.cpp:101` 创建服务。
- `src/low_speed_av_control/src/control_node.cpp:315` 实现回调。

留空字段表示沿用当前值。证据：`control_node.cpp:320` 至 `control_node.cpp:323`。

## 6. ControllerFactory

文件：`src/low_speed_av_control/src/controller_factory.cpp`

支持：

- `pure_pursuit`
- `stanley`
- `lqr`
- `mpc_sampler`

证据：`controller_factory.cpp:12` 至 `controller_factory.cpp:23`。

### Pure Pursuit

文件：`src/low_speed_av_control/src/pure_pursuit_controller.cpp`

流程：

1. 查最近轨迹点。
2. 根据车速计算 lookahead。
3. 找目标点。
4. 将目标点转到车辆坐标系。
5. 输出 `desired_curvature_1pm` 和 `steering_angle_rad`。

证据：`pure_pursuit_controller.cpp:57`。

### Stanley

文件：`src/low_speed_av_control/src/stanley_controller.cpp`

流程：

1. 查最近轨迹点。
2. 计算 heading error。
3. 计算参考航向系下的 signed lateral error。
4. 用 `atan2(k * lateral, abs(speed) + epsilon)` 计算修正。
5. 输出 steering 和 desired curvature。

证据：`stanley_controller.cpp:53`、`stanley_controller.cpp:57`。

### LQR

文件：`src/low_speed_av_control/src/lqr_controller.cpp`

当前实现不是 Stanley fallback。它使用二维误差状态：

```text
x = [e_y, e_psi]^T
```

离散模型：

```text
A = [[1, v*dt],
     [0, 1]]
B = [[0],
     [v*dt/wheel_base]]
```

主要步骤：

1. 空轨迹或全零速 stop trajectory 输出 emergency stop。
   - 证据：`lqr_controller.cpp:67`、`lqr_controller.cpp:73`。
2. 找最近点，并按 `preview_time_s` 取 preview reference。
   - 证据：`lqr_controller.cpp:84`、`lqr_controller.cpp:85`。
3. 计算 `e_y` 和 `e_psi`。
4. 用迭代 Riccati/DARE 计算 K。
   - 证据：`lqr_controller.cpp:100` 至 `lqr_controller.cpp:134`。
5. 使用曲率前馈 `atan(wheel_base * kappa_ref)`。
   - 证据：`lqr_controller.cpp:149`。
6. 反馈项 `delta_fb = -(Kx)`。
   - 证据：`lqr_controller.cpp:151`。
7. 限制到 `lqr.max_steering_angle_rad`。
8. 输出 `desired_curvature_1pm`。
   - 证据：`lqr_controller.cpp:158`。

### MPC sampler

文件：`src/low_speed_av_control/src/mpc_sampler_controller.cpp`

当前是轻量确定性采样器：

- 使用配置的 curvature samples，或按 sample_count 生成。
- 按 horizon 预测 pose。
- 计算 lateral、heading、speed、steering effort cost。
- 选择最小 cost 的 curvature。

证据：`mpc_sampler_controller.cpp:16`、`mpc_sampler_controller.cpp:51`、`mpc_sampler_controller.cpp:74`。

## 7. VehicleModelFactory

文件：`src/low_speed_av_control/src/vehicle_model_factory.cpp`

支持：

- `front_ackermann`
- `dual_ackermann`

### front_ackermann

文件：`src/low_speed_av_control/src/front_ackermann_model.cpp`

公式：

```text
kappa = tan(delta_front) / wheel_base
delta_front = atan(kappa * wheel_base)
rear = 0
```

证据：`front_ackermann_model.cpp:8`。

### dual_ackermann

文件：`src/low_speed_av_control/src/dual_ackermann_model.cpp`

公式：

```text
tan(delta_front) = kappa * wheel_base / (1 + rear_steer_ratio)
tan(delta_rear) = -rear_steer_ratio * tan(delta_front)
```

证据：`dual_ackermann_model.cpp:8`。

## 8. 正常控制周期

入口：`ControlNode::on_timer`，证据：`src/low_speed_av_control/src/control_node.cpp:285`。

步骤：

1. 如果 `safety_estop_active_`，发布 `controlled_stop("safety_estop")`。
2. 如果没有 pose 或定位超时，发布 `controlled_stop("localization_timeout")`。
3. 如果没有 trajectory 或轨迹超时，发布 `controlled_stop("trajectory_timeout")`。
4. 如果 trajectory 为空，发布 `controlled_stop("empty_trajectory")`。
5. 正常时执行 `compute_tracking_command()`。
6. `controller_->compute()` 输出内部 curvature/speed/gear。
7. `vehicle_model_->steering_from_curvature()` 转前/后轮转角。
8. `CommandLimiter::limit()` 做有限性检查、速度/加速度/转角限制。
9. `CommandSmoother::smooth()` 做速度步长和转角速率平滑。
10. `publish_command()` 根据 `output.mode` 发布内部命令和/或 SCU 命令。

证据：

- `control_node.cpp:289` Estop。
- `control_node.cpp:296` localization timeout。
- `control_node.cpp:303` trajectory timeout。
- `control_node.cpp:308` empty trajectory。
- `control_node.cpp:312` 正常 tracking。
- `control_node.cpp:257` tracking command。
- `control_node.cpp:280` limiter + smoother。
- `control_node.cpp:351` 发布命令。

## 9. 安全与 Estop

`/safety/status` 使用 `low_speed_av_interfaces/msg/ModuleStatus`。

触发条件：

- `level >= 2`
- `state == "estop"`
- `state == "emergency_stop"`
- `state == "failure"`

清除条件：

- 如果 `safety.estop_latched=false`，非触发状态会清除。
- 如果 latch=true，普通 `ok/standby` 仅表示当前请求撤销，不能清除锁存。
- 必须调用 `/low_speed_av_control/clear_estop`（`std_srvs/srv/Trigger`），且定位、轨迹、VehicleState、静止速度、自治许可、制动和故障条件全部满足。
- 清除后先进入 `READY`，下一控制周期再次验证输入。

证据：`src/low_speed_av_control/src/control_node.cpp` 的 `on_safety_status()`、`on_clear_estop()` 与 `safety_state_machine.cpp`。

## 10. Limiter、Smoother、NaN/Inf guard

`CommandLimiter`：

- 检查 speed、accel、curvature、steering、brake 是否有限。
- 非有限命令转换为 emergency stop，reason 为 `nan_or_inf_guard`。
- 限制速度、加速度、前后转角。

证据：`src/low_speed_av_control/src/command_limiter.cpp:8`、`src/low_speed_av_control/src/command_limiter.cpp:22`、`src/low_speed_av_control/src/command_limiter.cpp:34`。

`CommandSmoother`：

- 限制速度变化步长。
- 限制前后轮转角变化速率。
- emergency stop 绕过平滑，立即 speed 0、steering 0、brake 1。

证据：`src/low_speed_av_control/src/command_smoother.cpp:8`、`src/low_speed_av_control/src/command_smoother.cpp:22`。

## 11. SCU mapper

文件：`src/low_speed_av_control/src/scu_command_mapper.cpp`

映射规则：

| 内部字段 | SCU 字段 | 转换 |
|---|---|---|
| `gear=1` | `scu_shift_level_request=1` | D |
| `gear=2` | `scu_shift_level_request=3` | R |
| `gear=4` | `scu_shift_level_request=2` | N |
| 其他 gear | stop command | 不发布非法 shift |
| `speed_mps` | `scu_target_speed` | `abs(mps) * 3.6` km/h |
| `front_steering_angle_rad` | `scu_steering_angle_front` | rad -> deg -> sign |
| `rear_steering_angle_rad` | `scu_steering_angle_rear` | rad -> deg -> sign |
| `emergency_stop/brake/!enable` | brake stop | speed 0、steering 0、brake true |

证据：

- `scu_command_mapper.cpp:35` steering sanitize。
- `scu_command_mapper.cpp:50` speed sanitize。
- `scu_command_mapper.cpp:63` gear mapping。
- `scu_command_mapper.cpp:89` stop command。
- `scu_command_mapper.cpp:108` normal map。

## 12. 代码路径表

| 文件 | 类/函数 | 职责 | 输入 | 输出 |
|---|---|---|---|---|
| `control_node.cpp` | `ControlNode` | ROS2 控制节点 | 参数、topic、timer | command/status |
| `control_node.cpp` | `on_pose` | PoseStamped 转内部 pose | `/localization/pose` | `pose_` |
| `control_node.cpp` | `on_trajectory` | Trajectory msg 转内部轨迹 | `/planning/trajectory` | `trajectory_` |
| `control_node.cpp` | `on_safety_status` | Estop latch/clear | `/safety/status` | `safety_estop_active_` |
| `control_node.cpp` | `on_timer` | 控制主循环 | pose/state/trajectory/safety | control command |
| `controller_factory.cpp` | `ControllerFactory::create` | 创建控制器 | algorithm string | controller |
| `vehicle_model_factory.cpp` | `VehicleModelFactory::create` | 创建车辆模型 | model string | vehicle model |
| `lqr_controller.cpp` | `LqrController::compute` | LQR tracking | pose/state/trajectory/options | curvature command |
| `command_limiter.cpp` | `CommandLimiter::limit` | 限幅和 NaN guard | command/limits | safe command |
| `command_smoother.cpp` | `CommandSmoother::smooth` | 命令平滑 | command/options | smoothed command |
| `scu_command_mapper.cpp` | `ScuCommandMapper::map` | SCU 输出转换 | internal command | SCU command |
