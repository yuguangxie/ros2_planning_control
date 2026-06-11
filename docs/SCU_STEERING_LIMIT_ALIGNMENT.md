# SCU 转角限值对齐说明

## 背景

Ubuntu 复测中，倒车路线 `N0015 -> N0014` 曾观察到 SCU 前轮转角约 `-29.79 deg`。底盘 driver 默认安全范围为约 `27 deg`，因此控制输出必须在 SCU 出口层做最终保护。

## 当前规则

控制内部仍使用 SI 单位：

- speed: `m/s`
- steering: `rad`
- curvature: `1/m`

最终 `ScuCommandMapper` 才转换为底盘命令：

- speed: `km/h`
- steering: `deg`
- shift: D=1, N=2, R=3

默认配置已对齐到 27 度：

```yaml
scu:
  max_steering_angle_deg: 27.0
  overrange_policy: "clamp"
```

## overrange policy

`ScuCommandMapper` 的出口规则：

1. 非有限 steering：映射为 `0.0` 并 warning。
2. 有限但超范围 steering：
   - `overrange_policy: clamp` 时裁剪到 `[-max, +max]`。
   - 其他策略时映射为 `0.0`。
3. 非有限 speed：映射为 `0.0`。
4. 超范围 speed：
   - `clamp` 时裁剪到最大速度。
   - 其他策略时映射为 `0.0`。
5. shift 永远只发布 1/2/3；未知 gear 发布 brake stop。

## 安全停车

以下情况必须输出 brake stop：

- estop
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

## 复测重点

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0015', goal_node_id: 'N0014', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"

ros2 topic echo /yunle_chassis/control/scu_control_command
```

期望：

- `abs(scu_steering_angle_front) <= 27.0`
- `abs(scu_steering_angle_rear) <= 27.0`
- reverse shift 为 3
- speed 非负 km/h
