# Phase 13：Control端到端安全语义

## 1. 阶段定位

Phase 13首先修复的是安全信息在 `Planning -> Control -> SCU` 链路中被丢失的问题。此前Control主要消费轨迹点，Planning发布的failure状态、emergency标志以及车辆自治许可等消息级信息没有形成统一的控制门控。

阶段状态为 `PARTIALLY_COMPLETED`：Control侧目标已经实现，但用户随后要求恢复当时对`src/yunle_chassis`的修改，因此Chassis软件watchdog没有在本阶段交付。

## 2. 生产代码修改

主要修改集中在：

- [ControlNode](../../src/low_speed_av_control/src/control_node.cpp)
- [SafetyStateMachine](../../src/low_speed_av_control/src/safety_state_machine.cpp)
- [Control类型](../../src/low_speed_av_control/include/low_speed_av_control/control_types.hpp)
- [CommandSmoother](../../src/low_speed_av_control/src/command_smoother.cpp)
- [Control配置](../../src/low_speed_av_control/config/control_params.yaml)

### 2.1 完整消费Trajectory安全元数据

Control不再只复制`Trajectory.points`，而是保存并验证：

- `trajectory_id`；
- `source_package_id`；
- `status`；
- `emergency_stop`；
- 本地接收时间；
- 点集合及每个点的数值、挡位和`s_m`顺序。

默认只有正常状态`ok`能够进入跟踪。空ID、空轨迹、NaN/Inf、非法gear、明显非单调`s_m`、failure或emergency均在调用controller之前被拒绝。

具体作用是：安全行为不再依赖某个controller对“零速点”或“单点轨迹”的偶然处理。Pure Pursuit、Stanley、LQR、MPC sampler以及两种Ackermann车型会在同一个安全入口前被截断，统一输出停车。

### 2.2 建立显式安全状态机

新增ROS-independent状态机，状态包括：

- `WAIT_INPUTS`：关键输入尚未齐备；
- `READY`：输入有效，但尚未进入运动输出；
- `ACTIVE`：唯一允许调用正常controller的状态；
- `CONTROLLED_STOP`：输入故障或运行条件不满足时的受控停车；
- `ESTOP_LATCHED`：锁存急停。

状态机按安全优先级判定：

1. SafetyStatus急停或显式emergency trajectory；
2. VehicleState故障、自治关闭、人工制动；
3. 定位缺失、非法或超时；
4. 轨迹缺失、非法、Planning failure或超时；
5. 正常跟踪。

具体作用是把分散的`if`条件收敛成可诊断、可单测、优先级固定的决策入口，避免低优先级原因覆盖高优先级急停。

### 2.3 VehicleState形成运动许可门控

Control保存并使用：

- `autonomous_enabled`；
- `brake_pressed`；
- `fault_code`；
- VehicleState接收时间和数值有效性。

`vehicle_state.required=true`时，未收到或超时直接停车。即使`required=false`，只要已经收到VehicleState，自治关闭、踩制动、故障或非法数值都禁止继续输出运动命令。

具体作用是防止“规划和定位都正常，但车辆已经退出自动驾驶或人工正在制动”时Control仍继续给速度。

### 2.4 使用本地steady-clock实现输入超时

定位、轨迹和VehicleState超时使用本地receive time与`std::chrono::steady_clock`，而不是完全依赖消息header stamp。

具体作用是：

- 不因零时间戳误报；
- 不因ROS仿真时间暂停而失去本地watchdog；
- 不受系统墙上时间跳变影响。

### 2.5 锁存急停只能显式清除

普通`ok`或`standby` SafetyStatus不再自动清除急停锁存。新增标准服务：

```text
/low_speed_av_control/clear_estop
std_srvs/srv/Trigger
```

清除前至少检查：

- 最近SafetyStatus不再请求急停；
- 轨迹不再请求emergency；
- 定位和轨迹已经恢复且新鲜；
- VehicleState存在且有效；
- 车辆速度低于阈值；
- 无故障、未踩制动、自治许可有效。

清除成功后先进入`READY`，不会在service回调中直接跳变为运动命令。

具体作用是避免安全信号短暂恢复后车辆自动重新起步，并为操作员提供明确的恢复动作和拒绝原因。

### 2.6 统一停车输出

安全停车固定为：

- `speed_mps=0`；
- 前后轮转角使用零转角安全策略；
- `brake=1`；
- `enable=false`；
- `reason`非空；
- hard estop时`emergency_stop=true`；
- SCU target speed为0且brake enable为true。

Smoother对安全停车直接旁路，不会让正常速度或转角渐变延迟停车。

### 2.7 默认同时发布三个Control输出

默认`output.mode=both`，发布：

- `/control/command`；
- `/control/status`；
- `/yunle_chassis/control/scu_control_command`。

具体作用是同时满足内部标准Control合同、状态诊断和Yunle SCU接入需求，避免生产默认只发SCU而丢失规范要求的内部命令。

## 3. 配置和依赖变化

Control与Bringup两份配置同步增加或明确：

- `controller.allowed_trajectory_statuses`；
- `controller.trajectory_s_tolerance_m`；
- `vehicle_state.required`；
- `vehicle_state.timeout_s`；
- `safety.clear_speed_threshold_mps`；
- `output.mode: both`。

新增`std_srvs`依赖用于Trigger清除服务，新增`ament_cmake_gtest`测试依赖。没有新增或修改自定义msg/srv字段，也没有改变canonical AD Package合同。

## 4. 测试修改和实际证据

本阶段增加：

- production-linked `test_safety_state_machine`源码；
- `offline_phase13_safety_smoke.py`；
- 4 controllers × 2 vehicle models停车矩阵；
- timeout、VehicleState门控、latch/clear、READY interlock和双输出检查。

当前Windows环境当时没有ROS2、colcon或C++编译器，因此：

- Python离线检查实际通过；
- C++ gtest为`GENERATED_NOT_EXECUTED`；
- ROS2 build/test为`SKIPPED_ROS2_UNAVAILABLE`。

Python测试不能替代生产C++证据，但它确认了离线数据合同和预期安全矩阵没有明显漂移。

## 5. 关闭的问题与剩余边界

本阶段关闭或修复了：

- `CDX-P0-001`：Planning emergency/failure不再在Control入口丢失；
- `CDX-P1-001`：VehicleState形成控制门控；
- `CDX-P1-002`：普通OK不能自动清除锁存急停；
- `CDX-P1-003`：默认发布`/control/command`和`/control/status`。

没有关闭：

- `CDX-P0-002`：Chassis软件独立scheduler/watchdog未实现。

后续项目明确选择依赖“连续500 ms没有0x121时底盘硬件停车”的硬件机制。该选择不会改变Phase 13的事实：Control存活时必须主动周期发布安全stop；Control或Driver完全失联时才由硬件合同接管。

