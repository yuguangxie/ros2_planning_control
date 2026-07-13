# low_speed_av_simulation

该包提供两个明确分离的仿真用途：

- `path_replay`：沿 Planning path 直接回放 pose，用于快速检查路网、Planning 和 RViz。
- `control_closed_loop`：`/control/command` 驱动 production `low_speed_av_simulation_core`，plant 再发布 `/localization/pose` 与 `/vehicle/state`。该模式是低速软件在环（SIL），不是实车或 HIL。

## Closed-loop plant

ROS-independent core 使用中点（RK2）积分：

```text
x_dot   = v cos(yaw)
y_dot   = v sin(yaw)
yaw_dot = v (tan(front_steer) - tan(rear_steer)) / wheel_base
```

速度、加减速度、可选 jerk、前后轮转角与转角速率均有限制。`enable=false`、brake、emergency、command timeout、非有限输入、非 DRIVE gear 和 reverse 均 fail closed 到停车。reverse controller 未实现。

闭环节点发布：

- `/localization/pose`
- `/vehicle/state`
- `/simulation/status`
- `/simulation/diagnostics`
- `/simulation/pose_path`

诊断包含 command/localization/step cadence、横向与航向误差、goal 误差、timeout/non-finite 计数、停车响应和 stop reason。横向误差按实际轨迹线段投影计算。

## 配置与服务

`config/closed_loop_simulation_params.yaml` 默认 100 Hz plant、20 Hz localization/VehicleState、0.2 s command timeout、最大 1.0 m/s。Simulation 参数只是 SIL 模型，不声明真实 Yunle 底盘制动力。

服务：

- `/simulation/start`
- `/simulation/pause`
- `/simulation/reset`
- `/simulation/rewind_path`

reset 清空 plant state、command cache、monitor 和 pose history，并从初始 pose 重新开始。

## 测试

`test_simulation_core` 直接链接 production core，使用显式 dt 覆盖 22 项 plant 合同和 3 项 runtime monitor 合同。两个 bounded launch tests 分别覆盖 reset、ControlCommand 驱动、VehicleState、timeout 停车、诊断、QoS 后续发布和 clean exit。

整个包不创建 UDP socket、不发布 CAN 0x121，也不启动 Chassis Driver 或 keyboard control。
