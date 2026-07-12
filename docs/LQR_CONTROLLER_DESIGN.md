# LQR 轨迹跟踪控制器设计

## 目标
将 `low_speed_av_control` 中的 LQR 升级为可用的离散运动学自行车模型 LQR。该实现不依赖重型求解器，通过迭代离散代数 Riccati 方程计算反馈增益。

## 状态变量
```text
x = [e_y, e_psi]^T
```

误差定义：

```text
e_y = -sin(yaw_ref)*(x_vehicle - x_ref) + cos(yaw_ref)*(y_vehicle - y_ref)
e_psi = normalize_angle(yaw_vehicle - yaw_ref)
```

## 离散模型
```text
A = [[1, v*dt],
     [0, 1]]
B = [[0],
     [v*dt/wheel_base]]
```

速度 `v` 优先使用当前车辆速度；如果车辆速度不可用或接近 0，则使用参考轨迹速度。建模速度会用 `lqr.min_speed_mps` 下限夹紧，保证低速时 Riccati 求解有限。

## Riccati 迭代
```text
P_next = A^T P A - A^T P B (R + B^T P B)^-1 B^T P A + Q
K = (R + B^T P B)^-1 B^T P A
```

实现使用 2x2 矩阵显式计算，避免新增线性代数依赖。

## 控制律
```text
delta_ff = atan(wheel_base * kappa_ref)
delta_fb = -K * [e_y, e_psi]^T
delta_cmd = clamp(delta_ff + delta_fb, +/- max_steering_angle_rad)
desired_curvature_1pm = tan(delta_cmd) / wheel_base
```

LQR 输出内部期望曲率 `desired_curvature_1pm`。随后车辆模型负责转换为前/后轮物理转角：

- `front_ackermann`：前轮有效，后轮为 0。
- `dual_ackermann`：前后轮均有效，后轮按反相比例转向。

## 预瞄策略
LQR 使用最近参考点加配置预瞄时间：

```text
target_s = nearest.s_m + max(vehicle_speed, min_speed_mps) * preview_time_s
```

随后选择轨迹中第一个 `s_m >= target_s` 的点作为参考点。

## 停车轨迹
空轨迹或所有参考速度接近 0 的停车轨迹不会产生转向噪声，而是返回安全停车命令。

## 配置
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

## 调试建议
- 增大 `q_lateral_error` 会更积极地修正横向偏差。
- 增大 `q_heading_error` 会更积极地修正航向偏差。
- 增大 `r_steering` 会降低转向动作幅度。
- `use_curvature_feedforward=true` 时，零误差曲线路径会输出曲率前馈。

## 验证
无 ROS2 环境可运行：

```powershell
python scripts\offline_scu_lqr_smoke.py
```

真实 ROS2 环境还需要运行 `colcon build/test` 和 topic/service 集成验证。无 ROS2 环境中这些命令必须记录为 `SKIPPED_ROS2_UNAVAILABLE`。

## Phase 16 输入与周期边界

LQR 的 `control_dt_s` 由 Control steady-clock 实际周期提供，并受 smoother 的 dt 安全边界约束。所有状态、pose、trajectory 和参数在进入 Riccati 计算前必须有限且合法。当前未实现 reverse 专用误差模型，reverse trajectory 明确输出 `unsupported_reverse_tracking` 停车，不复用前进 LQR 假装支持倒车。
