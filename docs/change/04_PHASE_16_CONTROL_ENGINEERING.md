# Phase 16：Control工程化与硬件Watchdog协同

## 1. 阶段定位

Phase 16在`src/yunle_chassis`零修改前提下，完善Control参数安全、输出稳定性、controller输入鲁棒性、轨迹progress、运行诊断和ROS2回归源码，并明确与“连续500 ms没有CAN 0x121时底盘硬件停车”合同的边界。

阶段状态为`IMPLEMENTED_LOCALLY_PENDING_ROS2_CI_AND_HIL`。

## 2. 参数治理与fail-fast

新增ROS-independent配置聚合和校验：

- [control_runtime_helpers.hpp](../../src/low_speed_av_control/include/low_speed_av_control/control_runtime_helpers.hpp)
- [control_runtime_helpers.cpp](../../src/low_speed_av_control/src/control_runtime_helpers.cpp)

启动时统一验证：

- controller、vehicle model、output mode；
- 正常trajectory status allowlist；
- control/status rate和所有timeout；
- 车辆wheelbase、rear ratio、速度、加减速、转角和rate limit；
- Pure Pursuit、Stanley、LQR和MPC sampler参数；
- smoother的accel、decel、jerk、dt和前后轮rate；
- SCU速度/转角、sign、overrange policy、stop shift、灯光和模式；
- estop clear速度阈值；
- progress窗口和heading阈值；
- hardware watchdog诊断上界和证据状态。

非法安全参数抛出异常，使节点启动失败，不再静默进入`std::clamp`或运行时未定义状态。

当前正常trajectory status只支持`ok`。即使误把failure、emergency、estop或invalid加入allowlist，生产入口仍会fail closed。

## 3. 消除silent no-op YAML

删除或收敛此前“配置中存在、生产代码没有实际使用”的key，包括：

- localization pose type占位；
- hold last command；
- 未使用的车身外形参数；
- 固定每周期speed step；
- 单一前后轮共用steer rate；
- 未落地的emergency decel/stop mode；
- 被显式curvature sample list遮蔽的MPC sample count/max curvature。

新增`scripts/check_control_config_contract.py`，检查：

- production Control YAML；
- Bringup Control YAML；
- template Control YAML；

三者解析结果相同，并要求每个YAML leaf同时出现在Node的`declare_parameter`和`get_parameter`中。

当前实际结果为：

```text
yaml_leaves=76 declared=76 consumed=76
```

具体作用是防止操作员以为某个安全参数已经生效，而代码实际上完全没有读取。

## 4. 基于真实dt的Limiter/Smoother

[CommandSmoother](../../src/low_speed_av_control/src/command_smoother.cpp)从固定每周期步长改为接收实际steady-clock control interval：

- 分开最大加速度和最大减速度；
- 限制jerk；
- 独立限制前轮和后轮转角rate；
- 对过小、过大、非有限或负dt使用明确的安全clamp；
- 保存多周期状态；
- 提供显式`reset()`；
- 输出dt clamp、speed limit、jerk limit、前后轮rate limit等诊断。

具体作用是使控制输出限制与真实时间一致。20 ms周期偶发变成40 ms时，允许变化量会按实际dt计算，而不是继续套用一个隐式固定step。

### 安全旁路

只要命令满足以下任一条件：

- `emergency_stop=true`；
- `brake>0`；
- `enable=false`；

smoother立即输出零速、零加速度、零曲率、零前后转角、brake和disable，不执行normal smoothing。

具体作用是避免正常舒适性平滑拖延emergency或controlled stop。

## 5. Controller与Vehicle Model鲁棒性

四个controller共用production输入校验，统一处理：

- 空轨迹；
- 全零速停车轨迹；
- pose/state/trajectory NaN或Inf；
- 非法wheelbase或dt；
- 非Drive车辆状态；
- 车辆状态出现明显负速度；
- trajectory非法gear或负速度；
- 当前未支持的reverse tracking。

所有拒绝路径返回零速、brake、disable和稳定reason，不继续生成运动控制量。

Front Ackermann和Dual Ackermann对非法曲率、wheelbase、rear ratio和steering limit抛出异常，不再用静默clamp掩盖非法模型配置。

具体作用是把“controller碰到坏输入会输出什么”从算法偶然行为统一为fail-closed合同。

## 6. Control tracking progress

新增`TrackingProgressTracker`：

- 以`source_package_id + trajectory_id`作为identity；
- 保存单调progress index；
- 只返回受`forward_window_points`限制的局部轨迹；
- 搜索时结合heading和gear；
- 新trajectory、controller/model switch、estop clear和重新进入tracking时reset。

具体作用是防止回环轨迹上controller每周期全局搜索最近点并跳到后续回环，同时防止切换算法后沿用旧controller/smoother状态。

## 7. 发布周期与诊断

新增`ControlCadenceMonitor`，使用steady-clock统计：

- last interval；
- max interval；
- p95 interval；
- last publish age；
- cycle count和publish count；
- missed cycles；
- deadline misses；
- 是否出现达到硬件timeout声明的gap。

默认周期合同：

- Control rate：50 Hz；
- nominal period：20 ms；
- 软件deadline warning：100 ms；
- 项目方声明硬件timeout：500 ms。

配置校验不允许把硬件诊断上界设置为大于500 ms。检测到Control周期gap达到该上界后，节点恢复执行时先发布安全stop，并通过`READY` interlock重新验证输入。

`/control/status`在现有ModuleStatus字段内增加结构化文本诊断，包括controller/model、输入age、publish age、max/p95、missed/deadline、limiter/smoother状态和hardware watchdog证据状态。

具体作用是让“是否稳定维持远小于500 ms的发布周期”可观测，而不是只依赖50 Hz配置值推断。

## 8. 主动停车与硬件失联停车的边界

### Control仍存活

遇到Planning failure、定位/轨迹/VehicleState timeout、自治关闭、制动、fault或estop时，Control继续按周期发布：

- internal speed 0；
- `enable=false`；
- brake；
- 零转角安全策略；
- SCU target speed 0和brake enable。

这种情况不等待500 ms硬件timeout。

### 0x121完全消失

Control、DDS、Driver、主机或网络完全失效，导致0x121停止时，由项目声明的底盘硬件watchdog接管。

软件不声称能够决定：

- 硬件实际制动力；
- 停车距离；
- timeout后的shift/steering状态；
- 恢复后是否自动允许运动。

当前证据状态是`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`。完整台架流程见[500 ms硬件Watchdog验证规程](../HARDWARE_WATCHDOG_500MS_VALIDATION.md)。

## 9. 测试修改

Control production-linked测试扩展为：

| Target | Source cases | 主要作用 |
|---|---:|---|
| `test_controllers` | 6 | 四controller nominal、empty、zero、NaN、reverse和确定性 |
| `test_vehicle_command_pipeline` | 11 | 两车型、limiter、真实dt、jerk、rate、reset、SCU |
| `test_safety_state_machine` | 6 | timeout、VehicleState、latch/clear和4×2 emergency |
| `test_control_runtime_helpers` | 5 | 参数fail-fast、输入拒绝、progress和fake-clock cadence |

四个target都链接`low_speed_av_control` production library，共28个C++ `TEST`源码case。

新增Control-only ROS2 launch source，覆盖：

- 默认`output.mode=both`；
- internal与SCU周期输出；
- 四controller切换和reset；
- VehicleState门控；
- localization/trajectory/vehicle timeout；
- estop latch与clear；
- late subscriber；
- bounded process exit。

测试不启动Chassis Driver，不访问真实UDP或底盘。

## 10. 实际执行状态

当前Windows环境结果：

- unified offline runner：18/18 PASS；
- Control config contract：76/76/76 PASS；
- template/sample一致性：PASS；
- repository UTF-8/JSON/Markdown检查：PASS；
- clang-format门禁：PASS；
- `git diff --check`：PASS；
- 28个C++ case：`GENERATED_NOT_EXECUTED`；
- 7个Control launch case：`SKIPPED_ROS2_UNAVAILABLE`；
- 当前工作区CI：`CONFIGURED_NOT_EXECUTED`；
- 500 ms Bench/HIL：`HIL_NOT_EXECUTED`。

## 11. Finding作用与限制

Phase 16对以下问题提供了生产修改：

- `CDX-P1-004`：参数、limiter和smoother合同；
- `CDX-P2-006`：Control回环progress；
- `CDX-P2-010`：algorithm/trajectory切换reset；
- `CDX-P2-011`：PARK/unknown使用固定brake stop。

由于新C++、ROS2和full CI没有执行，上述多项仍是`PARTIALLY_FIXED`或`GENERATED_NOT_EXECUTED`。

`CDX-P0-002`保持：

```text
OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION
```

Phase 16没有实现Chassis软件watchdog，也没有修改`src/yunle_chassis`。硬件500 ms声明只有完成供应商资料核对、CAN抓包和wheels-off故障注入后，才能追加`MITIGATION_VERIFIED`，仍不能写成软件FIXED。

