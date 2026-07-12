# Planning / Control 后续优化文档索引

本文档集基于以下系统边界编写：Yunle 底盘硬件在连续 500 ms 未接收到 CAN `0x121` 控制报文时，由硬件独立触发停车。本轮后续路线不要求、也不允许修改 `src/yunle_chassis`。

## 文档列表

- `01_HARDWARE_WATCHDOG_AND_WORK_ROADMAP.md`：硬件 watchdog 边界、风险处置、Planning/Control 后续工作安排。
- `02_PHASE_15_PLANNING_INTEGRITY_PROMPT.md`：第一阶段完整 Prompt，聚焦 Planning 数据完整性、规划正确性和生产 C++ 测试。
- `03_PHASE_16_CONTROL_ENGINEERING_PROMPT.md`：第二阶段完整 Prompt，聚焦 Control 工程化、输出周期和 500 ms 硬件 watchdog 合同。
- `04_UBUNTU_HUMBLE_FULL_VALIDATION_PROMPT.md`：在 Ubuntu 22.04 + ROS2 Humble 上对 Phase 13—16 当前提交执行全量构建、production-linked C++、ROS2 integration、offline、sanitizer 和证据审计的完整 Prompt。

## 使用顺序

1. 先阅读硬件边界和工作路线。
2. 独立执行 Phase 15 Prompt，完成后停止并保存同提交证据。
3. 复核 Phase 15 后，再执行 Phase 16 Prompt。
4. 两阶段均完成后，才评估闭环仿真、高级算法和 HIL 扩展。
5. 在 Ubuntu 22.04 + ROS2 Humble 环境使用第4份 Prompt，对同一提交执行真实构建与测试并生成验证报告。

## 状态约束

- 硬件 watchdog 是系统级外部安全机制，不是 `chassis_driver` 软件功能。
- `CDX-P0-002` 的代码事实仍是“Driver 无独立软件 watchdog”；处置可记录为 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`，不得写成软件 `FIXED`。
- 未取得供应商协议、CAN 抓包或故障注入证据时，只能写“500 ms 硬件合同由项目方声明”，不能写“已验证通过”。
- Python smoke 不替代 production C++/ROS2 测试；CI 文件存在不等于 CI PASS。
