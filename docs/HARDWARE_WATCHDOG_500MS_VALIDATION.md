# 底盘 500 ms 硬件 Watchdog 验证规程

## 当前证据状态

`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`

“连续 500 ms 未收到 CAN 0x121 时底盘硬件停车”来自项目方声明。仓库中未发现供应商正式协议、固件适用矩阵、CAN 抓包、故障注入记录或安全负责人签字，因此本文件是待执行规程，不是 HIL 结果。

## 安全前提

- wheels-off、架车或专用低速台架；驱动轮不得接触人员或障碍物；
- 机械急停、独立断电和安全员到位；
- 明确车辆型号、VCU/SCU/网关固件、DBC 版本和配置 commit；
- CAN 记录工具能同时记录 0x121、硬件 timeout 状态和制动/shift/steering 反馈；
- 未经安全评审不得在开放道路执行。

## 软件主动停车对照

先保持 Control 与 Driver 存活，分别注入 Planning failure、定位/轨迹/VehicleState timeout、自治关闭、制动、fault 和 estop。期望 Control 继续以默认 50 Hz 发布 target speed 0、brake true、零转角；这些 case 不等待硬件 timeout。

## 失联故障注入矩阵

| Case | 操作 | 必须记录 | 验收字段 | 结果 |
|---|---|---|---|---|
| Normal | 正常 ACTIVE/stop 各运行一段时间 | 0x121 timestamp、interval max/p95 | 正常周期远小于 500 ms | HIL_NOT_EXECUTED |
| Control exit | 停止/杀死 Control publisher | 最后一条 ROS SCU、最后一帧 0x121、停车触发 | 触发延迟 <= 500 ms | HIL_NOT_EXECUTED |
| Driver exit | 杀死 Driver | 最后 0x121、停车触发 | 触发延迟 <= 500 ms | HIL_NOT_EXECUTED |
| Network loss | 断开主机到网关网络 | 最后 0x121、停车触发 | 触发延迟 <= 500 ms | HIL_NOT_EXECUTED |
| Host power loss | 台架受控断电 | 最后 0x121、停车触发 | 触发延迟 <= 500 ms | HIL_NOT_EXECUTED |

## 每个 Case 的证据

1. commit SHA、ROS distro、配置 hash、车辆/固件版本；
2. CAN 抓包原始文件及时间基准；
3. 最后一帧 0x121 时间 `t_last`；
4. 硬件停车/timeout 状态时间 `t_stop`；
5. `t_stop - t_last`、停车距离、速度、制动、shift、steering；
6. 0x121 恢复后是否自动允许运动、是否需要人工确认；
7. 操作员、安全员、底盘负责人签字。

只有所有适用 case 通过且证据归档后，才能把配置/报告状态改为 `HIL_VERIFIED / MITIGATION_VERIFIED`。该状态不等于 Chassis 软件 watchdog 已实现，`CDX-P0-002` 仍不得标记为软件 FIXED。
