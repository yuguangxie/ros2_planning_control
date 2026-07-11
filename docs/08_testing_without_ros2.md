# 无 ROS2 环境下的测试边界

## 测试分层

本项目明确区分四类验证，不能互相冒充：

| 类型 | 是否执行生产 C++ | 当前 Windows 环境 | 作用 |
|---|---:|---|---|
| C++ production unit test | 是，gtest 直接链接 production library/core | `GENERATED_NOT_EXECUTED` | Loader、Planner、Controller、Safety、SCU mapper、DBC、codec、frame builder |
| ROS2 launch/integration | 是，启动生产节点 | `SKIPPED_ROS2_UNAVAILABLE` | Planning failure trajectory 到 Control/SCU 的 topic/service 合同 |
| Python data-contract | 否 | 可执行 | AD Package、fixture、几何连续性、默认入口回归 |
| Template/config consistency | 否 | 可执行 | runtime/template/config/sample 漂移 |

Python smoke 不是 C++ 行为证明，也不能证明 ROS2 QoS、service、timeout 或节点生命周期。

## 统一离线入口

```powershell
& 'C:\Program Files\FreeCAD 1.2\bin\python.exe' scripts\run_offline_checks.py
```

也可以使用：

```powershell
uv run python scripts/run_offline_checks.py
```

runner 使用当前解释器逐项执行，保留子进程 stdout/stderr，输出每项 `PASS/FAIL/SKIPPED` 和总计；必要检查失败时返回非零退出码。它会显式验证 sample、正式 `_1`、正式 `_2`，并在临时目录构造空 `parking_points` fixture，不修改正式数据。

## Template 同步策略

- `scripts/` 是 offline validator 的 canonical source；`templates/offline_validation` 除 sample 默认路径外必须与 runtime 脚本一致。
- `src/low_speed_av_planning/config` 与 `src/low_speed_av_control/config` 是配置 canonical source；template YAML 的解析结果必须一致。
- `templates/sample_ad_package` 是样例包 canonical source；bringup sample 的文件集合和 SHA-256 必须一致。
- `scripts/check_template_consistency.py` 自动检查这些关系及 Phase 14 必要文件。

## 当前环境不能执行的项目

没有 ROS2、colcon、cmake 或 C++ compiler 时：

```text
C++ gtest source       = GENERATED_NOT_EXECUTED
ROS2 launch tests      = SKIPPED_ROS2_UNAVAILABLE
GitHub CI workflow     = CONFIGURED_NOT_EXECUTED
Chassis watchdog tests = SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002
```

真实 ROS2 Humble 环境应执行：

```bash
source /opt/ros/humble/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
python3 scripts/run_offline_checks.py
```

只有保存了当前 commit 对应的真实输出，才能写成 PASS。
