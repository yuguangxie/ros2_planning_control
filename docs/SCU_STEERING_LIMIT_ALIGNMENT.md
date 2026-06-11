# SCU 转角限值对齐说明

## 目标

统一控制模块、SCU mapper、bringup 配置和人工验证文档中的底盘转角限制，避免向 Yunle chassis driver 发布超过默认安全范围的前/后轮转角。

## Canonical 参数名

当前工程的正式参数名是：

```yaml
scu:
  max_steering_angle_deg: 27.0
  overrange_policy: "clamp"
```

请不要在新文档或复测命令中使用 `output.scu_max_steering_angle_deg` 或 `output.scu_overrange_policy`。`output.mode=scu_control_command` 仍然保留，用于选择输出模式；SCU 物理限值属于 `scu.*` 参数组。

## 单位合同

控制内部保持 SI 单位：

- speed: `m/s`
- steering: `rad`
- curvature: `1/m`

`ScuCommandMapper` 在最终输出层转换为底盘命令：

- `target_speed_mps -> scu_target_speed`：`abs(m/s) * 3.6`，单位 `km/h`
- `front_steering_angle_rad -> scu_steering_angle_front`：弧度转角度
- `rear_steering_angle_rad -> scu_steering_angle_rear`：弧度转角度
- gear：D=1，N=2，R=3

## 限幅策略

`scu.overrange_policy=clamp` 时：

- 有限但超范围的前/后轮转角裁剪到 `[-scu.max_steering_angle_deg, +scu.max_steering_angle_deg]`。
- 非有限转角映射为 `0.0` 并记录 warning。
- 有限但超范围速度按配置裁剪或置零；非有限速度映射为 `0.0`。
- shift 永远只发布 1/2/3，未知 gear 发布 brake stop。

## 安全停车

以下场景必须输出 brake stop：

- safety estop
- localization timeout
- trajectory timeout
- empty trajectory
- invalid command
- unknown gear

brake stop 字段：

```text
scu_brake_enable=true
scu_target_speed=0
scu_steering_angle_front=0
scu_steering_angle_rear=0
scu_shift_level_request=valid stop gear, default D=1
```

## Ubuntu 复测重点

```bash
ros2 param get /low_speed_av_control scu.max_steering_angle_deg
ros2 param get /low_speed_av_control scu.overrange_policy
```

期望：

```text
27.0
clamp
```

倒车路线复测：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0015', goal_node_id: 'N0014', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"

ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- `abs(scu_steering_angle_front) <= 27.0`
- `abs(scu_steering_angle_rear) <= 27.0`
- reverse shift 为 `3`
- speed 为非负 `km/h`

## ROS2 命令

本文档在 Windows Codex 环境更新，未执行 ROS2：

```text
SKIPPED_ROS2_UNAVAILABLE
```
