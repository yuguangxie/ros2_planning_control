# Phase 08 — Integrate Control ROS2 Node and Safety

请实现 `low_speed_av_control_node`。

要求：

1. 订阅 trajectory topic，默认 `/planning/trajectory`，可配置。
2. 订阅 localization pose topic，默认 `/localization/pose`，可配置。
3. 订阅 vehicle state topic，默认 `/vehicle/state`，可配置。
4. 订阅 safety status topic，默认 `/safety/status`，可配置；不存在时不应崩溃。
5. 发布 `/control/command`。
6. 发布 `/control/status`。
7. 控制频率默认 50Hz，可配置。
8. 支持 `SetControllerAlgorithm.srv`。
9. 支持 `controller.algorithm` 切换。
10. 支持 `vehicle.model` 切换。
11. localization timeout 默认 0.2s 后安全停车。
12. trajectory timeout 默认 0.5s 后安全停车。
13. safety estop 时安全停车。
14. command smoother 生效。
15. 输出包含 front/rear steering。

生成：

```text
src/low_speed_av_control/config/control_params.yaml
src/low_speed_av_control/launch/control.launch.py
```

创建：

```text
reports/phase_08_report.md
```
