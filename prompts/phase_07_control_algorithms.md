# Phase 07 — Implement Control Algorithms

请实现控制算法并提供工厂：

```text
pure_pursuit
stanley
lqr
mpc_sampler
```

要求：

1. 所有算法实现同一 `ControllerBase` 接口。
2. Pure Pursuit 完整实现：最近点、动态预瞄、目标点、曲率命令、速度命令。
3. Stanley 完整实现：最近点、航向误差、横向误差、低速 epsilon。
4. LQR 实现工程版：配置增益 + 曲率前馈，不要留空。
5. MPC sampler 实现轻量 deterministic sampler，不引入重型求解器。
6. 每个算法都通过 VehicleModelBase 生成 front/rear steering，不直接假设前轮转向。
7. 所有输出经过 CommandLimiter。
8. NaN/Inf 必须转换为安全停车。

更新 offline smoke：

- 从 sample trajectory 计算 Pure Pursuit 和 Stanley 控制命令。
- 检查输出有限且在转角限幅内。

创建：

```text
reports/phase_07_report.md
```
