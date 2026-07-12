# Phase 13—16四阶段总体说明

## 1. 四阶段解决问题的顺序

四个阶段不是四次互不相关的功能堆叠，而是按照“先保证安全语义，再建立证据底座，再分别加固Planning和Control”的顺序推进：

```text
Phase 13：先让危险状态真正能阻止运动
    ↓
Phase 14：让生产C++和ROS2行为有直接测试与CI入口
    ↓
Phase 15：加固Planning输入、搜索、semantic和trajectory
    ↓
Phase 16：加固Control参数、输出、controller、progress和周期诊断
```

## 2. 阶段对比

| 阶段 | 主要模块 | 核心修改 | 直接作用 | 当前证据状态 |
|---|---|---|---|---|
| Phase 13 | Control、安全接口 | Trajectory元数据、状态机、VehicleState、estop clear、双输出 | Planning failure/emergency和车辆禁止条件不能继续输出运动 | Control生产实现完成；ROS2/C++当时未执行 |
| Phase 14 | Planning/Control/Chassis测试、CI | production-linked gtest、launch source、统一runner、template检查、GitHub Actions | 后续修改能够直接回归生产实现，不再只依赖Python复刻 | offline PASS；历史CI sanitizer PASS、主job FAIL |
| Phase 15 | Planning | path containment、结构校验、A*/Dijkstra、semantic helper、route summary、progress | 恶意/损坏AD Package fail closed；路径与轨迹更正确、可重复 | 40 C++ cases生成未执行；offline 17/17 PASS |
| Phase 16 | Control | 参数fail-fast、真实dt smoother、controller fail-closed、progress、cadence、HIL规程 | 输出更稳定、坏输入不产生运动、周期相对500 ms边界可诊断 | 28 C++ cases生成未执行；offline 18/18 PASS；HIL未执行 |

## 3. 修改后的端到端运行路径

### 3.1 Planning加载阶段

```text
AD Package
  -> canonical contract检查
  -> 路径containment
  -> schema/version/validation/checksum
  -> ID/引用/数值/index一致性
  -> 构建TopologyGraph
```

作用：恶意路径、损坏索引、重复ID、负cost或非法数值不能进入规划运行期。

### 3.2 Planning搜索与轨迹阶段

```text
PlanRoute / PlanMission
  -> deterministic Dijkstra/A*
  -> semantic/current-pose anchor
  -> edge waypoint stitch
  -> terminal segment
  -> full reference summary
  -> windowed local trajectory
```

作用：相同输入得到稳定路径；route summary、full reference和local trajectory的长度、累计s与终点保持一致；回环路线不无约束跳进度。

### 3.3 Control安全入口

```text
Trajectory metadata + points
Localization
VehicleState
SafetyStatus
  -> SafetyStateMachine
  -> 只有ACTIVE才调用controller
```

作用：任何高优先级安全条件在controller之前截断，不依赖具体控制算法的偶然行为。

### 3.4 Control正常输出

```text
bounded progress trajectory
  -> selected controller
  -> vehicle model
  -> limiter
  -> real-dt smoother
  -> /control/command
  -> SCU command
```

作用：controller输入有界且可重复；车辆几何、速度、加减速、jerk和前后轮rate受到明确限制；正常输出默认50 Hz。

### 3.5 Control主动停车

```text
Planning failure / input timeout / vehicle gate / estop
  -> state machine stop
  -> bypass controller normal path
  -> bypass normal smoothing
  -> periodic zero-speed brake command
```

作用：Control仍存活时主动停车，不故意停止输出等待底盘超时。

### 3.6 完全失联

```text
Control / DDS / Driver / host / network failure
  -> CAN 0x121消失
  -> 项目声明底盘硬件在500 ms内停车
```

该路径没有由软件实现或纯ROS2测试证明。当前状态是`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`。

## 4. 测试体系的层次

### Production-linked C++ unit

- Planning：Loader、graph/planner、motion/speed、semantic/helper；
- Control：controllers、vehicle models、limiter/smoother、safety、SCU mapper、runtime helpers；
- Chassis：Phase 14现有DBC、codec和frame builder。

这些测试直接链接production target，不复制生产算法。Phase 15/16新增case在当前Windows环境尚未执行。

### ROS2 launch/integration

- Planning ready、service、mission、failure和QoS；
- Planning failure到Control brake stop；
- Control timeout、VehicleState、estop clear、algorithm switch、periodic output和late subscriber。

这些源码已注册，但当前环境没有ROS2，状态为`SKIPPED_ROS2_UNAVAILABLE`。

### Python data contract与治理

- sample、正式`_1`和正式`_2`验证；
- offline典型路线与fixture；
- 默认入口回归；
- config/template/sample一致性；
- UTF-8、JSON、Markdown link、expected tree和仓库卫生。

这些检查在当前环境实际PASS，但不能证明C++节点运行行为。

### Bench/HIL

- 0x121时间戳和jitter；
- Control退出；
- Driver退出；
- 网络中断；
- 主机断电；
- 实际硬件停车延迟、制动、shift、steering和恢复策略。

尚未执行，不能写成PASS。

## 5. 四阶段累计解决的主要问题

| Finding | 四阶段后的状态 | 说明 |
|---|---|---|
| `CDX-P0-001` | `FIXED_PRODUCTION` | Planning emergency/failure在Control controller前被消费；最新ROS2回归未在本地执行 |
| `CDX-P0-002` | `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION` | 没有Chassis软件watchdog；依赖尚未HIL验证的500 ms硬件合同 |
| `CDX-P1-001` | `FIXED` | VehicleState自治、制动和故障形成门控 |
| `CDX-P1-002` | `FIXED` | estop只能显式安全清除 |
| `CDX-P1-003` | `FIXED` | 默认同时发布internal command、status和SCU command |
| `CDX-P1-004` | `PARTIALLY_FIXED` | 参数和真实dt pipeline已实现；新C++/ROS2未执行 |
| `CDX-P1-005` | `PARTIALLY_FIXED` | Planning containment已实现；新C++负例未执行 |
| `CDX-P1-006` | `PARTIALLY_FIXED` | production-linked测试规模显著增加；新case未全部实际运行 |
| `CDX-P1-007` | `EXECUTED_FAIL / CURRENT_CONFIGURED_NOT_EXECUTED` | 没有当前工作区同提交full CI PASS |
| `CDX-P2-003/004/005/006` | `PARTIALLY_FIXED` | Planning结构、搜索、summary和progress已有生产修改，待C++/ROS2证据 |
| `CDX-P2-007` | `OPEN_CAPABILITY / FAIL_CLOSED_MITIGATION` | Reverse控制未实现，当前明确停车 |
| `CDX-P2-010` | `PARTIALLY_FIXED` | Control切换/reset已实现，待运行回归 |

## 6. 兼容性边界

四阶段保持：

- Planning、Control、Interfaces、Bringup、Simulation、Chassis职责分离；
- 自定义Roadnet架构，不使用Nav2/Lanelet2替代；
- canonical AD Package仍是`project_manifest.json`、`trajectory/waypoints.yaml`和`validation/validation_report.json`；
- 没有引入旧manifest/waypoints/validation路径；
- 没有修改现有自定义msg/srv字段；
- 默认速度保持低速保守；
- 没有修改正式Roadnet数据；
- Phase 15和Phase 16均保持`src/yunle_chassis`零diff。

Phase 14对Chassis做过可测试性core提取，但没有改变CAN ID、DBC mapping、topic、网关默认地址或单次callback发送语义，也没有实现软件watchdog。

## 7. 当前最重要的后续工作

1. 在Ubuntu ROS2 Humble对当前同一提交运行定向和全量colcon build/test/result。
2. 修复实际编译、IDL、QoS、launch timeout或flaky问题，取得required GitHub Actions full PASS。
3. 在wheels-off安全台架执行500 ms硬件watchdog故障注入，归档供应商合同、固件版本、CAN原始数据和签字记录。
4. 只有上述证据齐全后，才重新评估相关finding是否可以关闭。
5. 在此之前不要把Python PASS、测试源码存在或workflow配置文件存在描述为生产C++、ROS2、CI或HIL已经通过。

