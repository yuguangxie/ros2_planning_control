# Phase 06 — Vehicle Model and Control Foundation

请生成或更新 `low_speed_av_control` 的基础结构。

实现：

```text
VehicleModelBase
FrontAckermannModel
DualAckermannModel
ControlCommand internal struct
CommandLimiter
TrackingErrorEstimator
```

要求：

1. `front_ackermann` 支持 `kappa = tan(delta_front)/wheel_base`。
2. `dual_ackermann` 支持 counter-phase：`kappa = (tan(delta_front)-tan(delta_rear))/wheel_base`。
3. `rear_steer_ratio` 可配置。
4. 输出前轮和后轮转角。
5. 限制最大前轮转角、最大后轮转角、前/后转角速度、速度、加速度、减速度。
6. 所有车辆参数来自 `control_params.yaml` 或 `vehicle_params.yaml`。
7. 不从 AD Package 的 `wheel_base_m=null` 直接使用默认值；如果 package 中有 vehicle_profile，仅用于一致性 warning。

创建：

```text
reports/phase_06_report.md
```
