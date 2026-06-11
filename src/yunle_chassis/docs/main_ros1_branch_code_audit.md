# main 与 ros1 分支代码功能对比及审计报告

生成日期：2026-05-28

## 1. 审查范围

本报告只审查 `main` 和 `ros1` 两个分支的代码、配置、launch、消息定义和文档结构，不修改业务代码。

| 分支 | 当前提交 | 定位 |
|---|---|---|
| `main` | `7541bc5` | ROS2 / `ament_cmake` / `rclcpp` 版本 |
| `ros1` | `073c19b` | ROS1 / `catkin` / `roscpp` 版本 |

实际检查的主要命令：

```bash
git diff --name-status main..ros1
git diff --stat main..ros1
git ls-tree -r --name-only main
git ls-tree -r --name-only ros1
git diff --exit-code main..ros1 -- Yunle_CAN_release.dbc
git diff -- main..ros1 -- chassis_interfaces/msg
git grep -n "sendControlFrame\|rxLoop\|publishDecoded" main ros1 -- chassis_driver/src
```

## 2. 总体结论

`main` 和 `ros1` 两个分支的底盘驱动功能目标基本一致：均通过 UDP 以太网转 CAN 网关收发 13 字节 CAN 记录，均使用同一份 `Yunle_CAN_release.dbc` 固化后的 C++ DBC 编解码逻辑，均发布同一组底盘反馈话题，并订阅同一组控制话题。

两分支的主要差异来自 ROS 中间件适配：

| 维度 | `main` | `ros1` | 是否功能等价 |
|---|---|---|---|
| ROS 版本 | ROS2 | ROS1 | 是，属于中间件差异 |
| 构建系统 | `ament_cmake` | `catkin` | 是，属于构建差异 |
| 节点 API | `rclcpp::Node` | `ros::NodeHandle` 封装类 | 是 |
| 参数格式 | `chassis_driver_node.ros__parameters` 嵌套 YAML | 节点私有参数扁平 YAML | 是 |
| launch | Python launch | XML launch | 是 |
| topic 语义 | `/yunle_chassis/...` | `/yunle_chassis/...` | 是 |
| 消息字段 | `chassis_interfaces/msg/*.msg` | 同名 ROS1 msg 文件 | 是，字段内容无差异 |
| DBC 文件 | `Yunle_CAN_release.dbc` | 同一内容 | 是 |
| 键盘控制节点 | `keyboard_scu_control_node` | `keyboard_scu_control_node` | 是 |

结论：两个分支当前功能内容可以认为一致；差异主要是 ROS2 与 ROS1 的工程适配方式，不是协议或业务功能差异。

## 3. 文件级对比

| 文件/目录 | `main` 状态 | `ros1` 状态 | 对比结论 |
|---|---|---|---|
| `Yunle_CAN_release.dbc` | 存在 | 存在 | 内容一致，已通过 `git diff --exit-code main..ros1 -- Yunle_CAN_release.dbc` 确认。 |
| `chassis_interfaces/msg/*.msg` | ROS2 消息定义 | ROS1 消息定义 | 字段内容一致，`git diff -- main..ros1 -- chassis_interfaces/msg` 无输出。 |
| `chassis_interfaces/CMakeLists.txt` | `rosidl_generate_interfaces()` | `add_message_files()` / `generate_messages()` | 构建系统差异，功能等价。 |
| `chassis_interfaces/package.xml` | `format=3`，`ament_cmake` | `format=2`，`catkin` | ROS 版本差异，维护者均为 `skywilling`。 |
| `chassis_driver/CMakeLists.txt` | 创建 ROS2 可执行文件并声明 ament 依赖 | 创建 ROS1 可执行文件并链接 catkin 依赖 | 构建系统差异，目标节点一致。 |
| `chassis_driver/package.xml` | ROS2 依赖：`rclcpp` 等 | ROS1 依赖：`roscpp` 等 | ROS 版本差异，功能目标一致。 |
| `chassis_driver/config/chassis_driver.yaml` | ROS2 参数嵌套在 `chassis_driver_node.ros__parameters` 下 | ROS1 私有参数扁平结构 | 参数项一致，格式不同。 |
| `chassis_driver/launch/chassis_driver.launch.py` | ROS2 Python launch | 不存在 | 与 ROS2 运行方式匹配。 |
| `chassis_driver/launch/chassis_driver.launch` | 不存在 | ROS1 XML launch | 与 ROS1 运行方式匹配。 |
| `chassis_driver/launch/keyboard_scu_control.launch.py` | ROS2 Python launch | 不存在 | 与 ROS2 运行方式匹配。 |
| `chassis_driver/launch/keyboard_scu_control.launch` | 不存在 | ROS1 XML launch | 与 ROS1 运行方式匹配。 |
| `chassis_driver/src/can_ethernet_codec.cpp` | 13 字节网关帧编解码 | 同逻辑 | 功能一致；发送帧 bit5 保留位均保持 0。 |
| `chassis_driver/src/chassis_driver_node.cpp` | ROS2 节点、参数、发布订阅和线程 | ROS1 节点封装、参数、发布订阅和线程 | 功能一致，ROS API 不同。 |
| `chassis_driver/src/control_command_bridge.cpp` | ROS2 subscriber callback 封装控制 CAN | ROS1 subscriber callback 封装控制 CAN | 控制逻辑一致。 |
| `chassis_driver/src/keyboard_scu_control_node.cpp` | ROS2 键盘控制节点 | ROS1 键盘控制节点 | 行为一致，ROS API 不同。 |
| `docs/ros2_topic_reference.md` | ROS2 话题文档 | 不存在 | 分支专用文档。 |
| `docs/ros1_topic_reference.md` | 不存在 | ROS1 话题文档 | 分支专用文档。 |

## 4. 主要功能一致性

### 4.1 以太网转 CAN 通信

两分支均使用：

- CAN1 本地端口：`8234`
- CAN2 本地端口：`8235`
- CAN1 默认远端 IP：`192.168.1.98`
- CAN2 默认远端 IP：`192.168.1.99`
- CAN1 远端端口：`can1_remote_port=1234`
- CAN2 远端端口：`can2_remote_port=1234`
- UDP payload 中每个 CAN record 固定 13 字节

关键源码：

| 分支 | 位置 | 说明 |
|---|---|---|
| `main` | `chassis_driver/src/can_ethernet_codec.cpp:47` | `encodeFrame()` 生成 13 字节记录。 |
| `ros1` | `chassis_driver/src/can_ethernet_codec.cpp:47` | 同逻辑。 |
| `main` | `chassis_driver/src/can_ethernet_codec.cpp:51` | 信息字节由扩展帧位和 DLC 组成，未置 bit5。 |
| `ros1` | `chassis_driver/src/can_ethernet_codec.cpp:51` | 同逻辑。 |

### 4.2 反馈解析与发布

两分支均解析并发布：

| CAN ID | 报文名 | ROS topic |
|---|---|---|
| `0x100` | `BMS_Status` | `/yunle_chassis/feedback/bms_status` |
| `0x077` | `VCU_Warning_Level` | `/yunle_chassis/feedback/vcu_warning_level` |
| `0x168` | `VCU_Wheel_Speed_Feedback` | `/yunle_chassis/feedback/wheel_speed` |
| `0x051` | `VCU_CCU_Status` | `/yunle_chassis/feedback/ccu_status` |
| `0x0E1` | `SAS_Angle_Feedback` | `/yunle_chassis/feedback/sas_angle` |
| `0x7F1` | `SCU_Target_Speed_Feedback` | `/yunle_chassis/feedback/target_speed_feedback` |

两分支均将 `CCU_Steering_Wheel_Angle`、`SAS_Front_Angle` 和 `SAS_Rear_Angle` 按 `scu_control_max_steering_angle_deg` 换算为实际角度。

关键源码：

| 分支 | 位置 | 说明 |
|---|---|---|
| `main` | `chassis_driver/src/chassis_driver_node.cpp:310` | `publishDecoded()` 分发已知 CAN ID。 |
| `ros1` | `chassis_driver/src/chassis_driver_node.cpp:295` | 同逻辑。 |
| `main` | `chassis_driver/src/chassis_driver_node.cpp:340` | CCU 转角编码换算为实际角度。 |
| `ros1` | `chassis_driver/src/chassis_driver_node.cpp:325` | 同逻辑。 |
| `main` | `chassis_driver/src/chassis_driver_node.cpp:363` | SAS 前后转角换算。 |
| `ros1` | `chassis_driver/src/chassis_driver_node.cpp:348` | 同逻辑。 |

### 4.3 控制指令下发

两分支均订阅：

| ROS topic | 消息类型 | CAN 输出 |
|---|---|---|
| `/yunle_chassis/control/scu_control_command` | `ScuControlCommand` | `0x121 SCU_Control_Command` |
| `/yunle_chassis/control/scu_chassis_command` | `ScuChassisCommand` | `0x126 SCU_Chassis_Command` |
| `/yunle_chassis/control/scu_torque_command` | `ScuTorqueCommand` | `0x123 SCU_Torque_Command` |
| `/yunle_chassis/control/vcu_chassis_debug` | `VcuChassisDebug` | `0x710 VCU_Debug_Enable` 与 `0x715 VCU_Drive_Debug` |

`ScuControlCommand` 封装行为一致：

- `SCU_Drive_Mode_Request` 固定下发 `1`。
- `scu_shift_level_request` 只接受 `1=D`、`2=N`、`3=R`。
- 速度非有限、负值或超过 `scu_control_max_target_speed_kmh` 时，按 `0` 下发并输出警告。
- 前后转角非有限或超出 `±scu_control_max_steering_angle_deg` 时，按 `0` 下发并输出警告。

关键源码：

| 分支 | 位置 | 说明 |
|---|---|---|
| `main` | `chassis_driver/src/control_command_bridge.cpp:47` | 档位非法时丢弃 0x121 控制帧。 |
| `ros1` | `chassis_driver/src/control_command_bridge.cpp:44` | 同逻辑。 |
| `main` | `chassis_driver/src/control_command_bridge.cpp:56` | 速度越界归零。 |
| `ros1` | `chassis_driver/src/control_command_bridge.cpp:52` | 同逻辑。 |
| `main` | `chassis_driver/src/control_command_bridge.cpp:64` | 前轮转角越界归零。 |
| `ros1` | `chassis_driver/src/control_command_bridge.cpp:60` | 同逻辑。 |
| `main` | `chassis_driver/src/control_command_bridge.cpp:98` | 下发 `SCU_Control_Command`。 |
| `ros1` | `chassis_driver/src/control_command_bridge.cpp:94` | 同逻辑。 |

### 4.4 键盘控制节点

两分支键盘节点行为一致：

- 默认 N 档、零速度、零转角、制动关闭。
- `scu_torque_or_speed_mode=false/0`。
- `steering_angle_speed_valid=false`。
- `brake_force_command_valid=false`。
- `w/s` 只调整速度，不切换档位。
- `1/2/3` 单独选择 D/N/R 档。
- `h` 输出中英文帮助。

关键源码：

| 分支 | 位置 | 说明 |
|---|---|---|
| `main` | `chassis_driver/src/keyboard_scu_control_node.cpp:128` | 默认控制消息初始化。 |
| `ros1` | `chassis_driver/src/keyboard_scu_control_node.cpp:124` | 同逻辑。 |
| `main` | `chassis_driver/src/keyboard_scu_control_node.cpp:171` | `w` 增加速度，不改变档位。 |
| `ros1` | `chassis_driver/src/keyboard_scu_control_node.cpp:167` | 同逻辑。 |
| `main` | `chassis_driver/src/keyboard_scu_control_node.cpp:180` | `1/2/3` 切换档位。 |
| `ros1` | `chassis_driver/src/keyboard_scu_control_node.cpp:176` | 同逻辑。 |

## 5. `main` 分支代码与功能审计

### 5.1 当前功能

`main` 是 ROS2 版本，核心功能如下：

- 使用 `rclcpp` 创建 `chassis_driver_node`。
- 通过两路 UDP socket 连接 CAN1/CAN2 网关。
- 后台接收线程解析 UDP payload 为 CAN 帧。
- 解析底盘反馈 CAN 报文并发布 ROS2 topic。
- 订阅 ROS2 控制 topic 并封装为 CAN 报文下发。
- 支持 raw RX/TX CAN frame topic。
- 支持未知 CAN ID debug topic。
- 支持控制 CAN 帧十六进制日志。
- 支持键盘控制节点。

### 5.2 急需调整项

| 优先级 | 问题 | 影响 | 位置 |
|---|---|---|---|
| 高 | 缺少控制指令超时保护。 | 上层控制节点停止发布后，driver 不会主动下发停车/制动命令，安全链路依赖外部系统。 | `main:chassis_driver/src/control_command_bridge.cpp:98`，`main:chassis_driver/src/chassis_driver_node.cpp:288` |
| 高 | 0x121 控制报文不是 driver 内部 10 ms 周期发送。 | 协议要求或车辆控制器期望周期输入时，callback 触发式发送可能导致控制不连续。 | `main:chassis_driver/src/control_command_bridge.cpp:98` |
| 高 | 连接异常后没有停车或自动降级逻辑。 | UDP send/receive 异常只记录错误或静默失败，车辆状态处置依赖外部。 | `main:chassis_driver/src/chassis_driver_node.cpp:288`，`main:chassis_driver/src/udp_channel.cpp:72` |
| 中 | `message_channel_map` 已加载但未用于 RX 通道过滤。 | 如果 CAN1/CAN2 收到重复或串线报文，当前会直接按 CAN ID 发布，配置的反馈通道路由没有实际约束。 | `main:chassis_driver/src/frame_router.cpp:11`，`main:chassis_driver/src/chassis_driver_node.cpp:189` |
| 中 | UDP 接收未校验发送源 IP/端口。 | 任何发到本地端口的 UDP payload 都可能被解析为 CAN 帧，调试网络中存在误注入风险。 | `main:chassis_driver/src/udp_channel.cpp:65` |
| 中 | UDP 端口参数缺少范围校验。 | 负数或超过 65535 的端口会在 `static_cast<uint16_t>` 时截断，配置错误不易发现。 | `main:chassis_driver/src/chassis_driver_node.cpp:228` |
| 中 | 高风险控制 topic 缺少启用闸门。 | 扭矩控制、VCU PID 调试只依赖 topic 开关和 DBC clamp，误发布风险较高。 | `main:chassis_driver/src/control_command_bridge.cpp:114`，`:129`，`:141` |
| 低 | 缺少单元测试和协议回归测试。 | DBC 位域、端序、缩放和边界值修改后不易验证。 | `main:chassis_driver/src/dbc_protocol.cpp`，`main:chassis_driver/src/can_ethernet_codec.cpp` |

### 5.3 可优化项

- 将 CAN ID、周期、消息名到 topic 的映射集中到一个表或生成层，减少 `publishDecoded()` 中硬编码分支。
- 为 `CanEthernetCodec` 增加测试：标准帧/扩展帧、DLC、bit5 保留位、多个 13 字节 record、尾部脏字节。
- 为 `DbcProtocol` 增加测试：有符号/无符号、缩放、补码、跨字节信号。
- 增加运行状态 topic，明确网关连接状态、最后接收时间、最后发送时间、连续错误次数。
- 对 `scu_chassis_command`、`scu_torque_command`、`vcu_chassis_debug` 增加独立安全参数，例如 `enable_torque_command`、`enable_vcu_debug_command`，默认关闭。
- 对 `scu_torque_or_speed_mode`、灯光请求等枚举值增加显式范围校验和警告。

## 6. `ros1` 分支代码与功能审计

### 6.1 当前功能

`ros1` 是 ROS1 移植版本，功能与 `main` 对齐：

- 使用 `roscpp` 创建 `chassis_driver_node`。
- 使用 catkin 构建自定义消息和驱动节点。
- 使用 XML launch 加载私有参数。
- 两路 UDP CAN 网关收发。
- 底盘反馈解析和 ROS1 topic 发布。
- 控制 topic 封装为 CAN 帧下发。
- raw/debug topic、控制 CAN 十六进制日志、键盘控制节点均已移植。

### 6.2 急需调整项

`ros1` 与 `main` 共享同样的主要安全和实时性短板：

| 优先级 | 问题 | 影响 | 位置 |
|---|---|---|---|
| 高 | 缺少控制指令超时保护。 | 上层停止发布控制 topic 后，driver 不会主动停车。 | `ros1:chassis_driver/src/control_command_bridge.cpp:94`，`ros1:chassis_driver/src/chassis_driver_node.cpp:273` |
| 高 | 0x121 控制报文不是 driver 内部 10 ms 周期发送。 | 控制连续性取决于上层 topic 发布频率。 | `ros1:chassis_driver/src/control_command_bridge.cpp:94` |
| 高 | 连接异常后没有停车或自动重连策略。 | 网关异常时 driver 只表现为接收不到数据或发送失败。 | `ros1:chassis_driver/src/chassis_driver_node.cpp:273`，`ros1:chassis_driver/src/udp_channel.cpp:72` |
| 中 | `message_channel_map` 未用于 RX 通道过滤。 | 配置记录了反馈通道，但接收发布不受该配置约束。 | `ros1:chassis_driver/src/frame_router.cpp:11`，`ros1:chassis_driver/src/chassis_driver_node.cpp:176` |
| 中 | UDP 接收未校验发送源 IP/端口。 | 非预期 UDP 数据可能被当作 CAN 网关数据处理。 | `ros1:chassis_driver/src/udp_channel.cpp:65` |
| 中 | UDP 端口参数缺少范围校验。 | 错误端口值可能被截断后使用。 | `ros1:chassis_driver/src/chassis_driver_node.cpp:213` |
| 中 | 高风险控制 topic 缺少启用闸门。 | 扭矩控制和 VCU 调试控制误操作风险较高。 | `ros1:chassis_driver/src/control_command_bridge.cpp:110`，`:125`，`:137` |
| 低 | 缺少 ROS1 构建和回归测试记录。 | 移植后不易确认消息生成、launch 和键盘节点在 Noetic 环境下可用。 | `ros1:chassis_driver/CMakeLists.txt` |

### 6.3 可优化项

- 增加 ROS1 专用 launch 参数示例，覆盖键盘节点常用参数。
- 增加 `rostest` 或最小 catkin 单元测试，覆盖 codec 和 DBC 编解码。
- 将 ROS1 与 ROS2 的共同协议/编码逻辑抽成可共享目录或生成文件，降低双分支维护成本。
- 在 README 中增加 ROS1 与 ROS2 分支选择建议，避免用户在错误 ROS 环境使用错误分支。

## 7. 两分支共同风险

### 7.1 安全链路不足

当前 driver 更像“协议桥接节点”，不是完整安全控制器。它对输入范围做了基础保护，但没有形成独立的安全闭环。

建议优先补齐：

1. 控制 topic watchdog。
2. 0x121 周期发送机制。
3. 超时停车或制动命令。
4. 网关发送失败和接收超时状态发布。
5. 高风险控制 topic 的显式启用参数。

### 7.2 接收链路缺少来源约束

`UdpChannel::receive()` 接收 UDP 包后没有检查 `src` 是否等于配置的网关 IP/端口。该行为在封闭网络中通常可工作，但在测试网络、多设备同网段或抓包回放环境中可能导致误解析。

建议：

- 默认只接受来自配置远端 IP 的 UDP。
- 可选增加 `strict_remote_endpoint` 参数。
- 对被丢弃的非预期来源数据做限频 warning。

### 7.3 `message_channel_map` 语义未闭环

`message_channel_map` 当前只是被加载到 `feedback_channel_map_`，但 `FrameRouter::routeFrame()` 直接调用 `publishDecoded(frame)`，没有按报文名和通道过滤。文档中对“通道映射”的描述可能让用户以为 RX 已过滤。

建议：

- 要么实现 RX 通道过滤。
- 要么在文档中明确 `message_channel_map` 目前只作为配置记录，不参与过滤。

### 7.4 缺少测试

两分支均缺少自动化测试。当前协议驱动非常依赖位域、缩放、端序和边界值，建议至少增加纯 C++ 单元测试，不依赖真实 ROS master 或底盘。

建议优先测试：

- `CanEthernetCodec::encodeFrame()` / `decodePayload()`。
- `DbcProtocol::encodeSignal()` / `decodeSignal()`。
- `ScuControlCommand` 超范围归零逻辑。
- 转角编码值到实际角度换算。

## 8. 后续建议优先级

| 优先级 | 建议 | 适用分支 |
|---|---|---|
| P0 | 增加控制超时 watchdog 和停车/制动策略 | `main`、`ros1` |
| P0 | 实现 0x121 周期发送，周期可配置，默认按协议 10 ms | `main`、`ros1` |
| P1 | 增加 UDP 来源 IP/端口校验和连接状态 topic | `main`、`ros1` |
| P1 | 高风险控制 topic 增加显式启用参数，默认关闭 | `main`、`ros1` |
| P1 | 明确或实现 `message_channel_map` 的 RX 过滤语义 | `main`、`ros1` |
| P2 | 增加 codec/DBC/control wrapper 单元测试 | `main`、`ros1` |
| P2 | 将共同协议层抽象为可共享代码或生成文件 | `main`、`ros1` |
| P3 | 改善 README 中 ROS1/ROS2 版本选择说明 | `main`、`ros1` |

## 9. 本次审计未做事项

- 未修改任何业务代码。
- 未切换或改写 `ros1` 分支内容。
- 未运行 ROS2 `colcon build`，当前请求是审计并生成文档。
- 未运行 ROS1 `catkin_make`，当前工作环境未确认 ROS1/ROS2 运行环境。
- 未连接真实底盘或以太网转 CAN 网关验证运行时行为。
