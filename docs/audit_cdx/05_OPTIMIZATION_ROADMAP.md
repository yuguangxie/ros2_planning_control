# 后续优化路线图

## 阶段 0：冻结实车运动测试

在 CDX-P0-001、CDX-P0-002 未关闭前，只允许离线、仿真、消息监控和受控 bench-only。文档中所有“安全失败路径通过”应增加限定：Control 必须真实消费 emergency flag，Chassis 必须具备独立 watchdog。

完成定义：安全负责人确认禁行边界；当前 commit、配置和测试结果都有唯一版本号。

## 阶段 1：修复端到端安全语义

工作项：

1. Control 保存 trajectory metadata，拒绝 emergency/failure/非法 status。
2. 建立明确的 control enable 状态机，接入 autonomous enabled、brake、fault、safety、input freshness。
3. estop 只允许显式 clear；恢复前要求车辆静止。
4. Chassis 增加启动/timeout/shutdown brake 和周期 command scheduler。
5. 默认 `output.mode=both`，或正式修改接口合同。

完成定义：四种 controller、两种 vehicle model、Planning failure、Control timeout、Control crash、DDS 断链均能在 CAN capture 中看到确定的 brake stop。

## 阶段 2：建立可相信的测试底座

工作项：

- 为 loader、planner、controller、mapper、codec 建 gtest。
- 给 Planning/Control/Chassis 注册 ament tests。
- 增加 launch_testing 端到端用例。
- 将现有 Python smoke 定位为 fixture/data contract test，不再替代 C++ test。
- CI 在 ROS2 Humble 容器运行 rosdep、build、test、test-result、lint、ASan/UBSan。

完成定义：PR 必须通过 C++ 单测和 ROS2 集成测试；保存 coverage 与 test-result；安全关键分支有负例。

## 阶段 3：收紧 AD Package 与规划正确性

工作项：

- 阻止 manifest path/symlink escape。
- 校验 schema、重复 ID、数值范围、edge/index/waypoint 一致性。
- RoadnetStatus 保留真实 validation `passed/warning`。
- A* 保证 admissible heuristic 或明确 weighted A* 语义。
- 最近点匹配增加进度窗口、heading、gear 和 loop 处理。
- 修正 semantic terminal 的 route length/time。

完成定义：恶意/损坏包全部 fail closed；Dijkstra/A* 在随机图和正式包上结果可重复；路径摘要与轨迹几何一致。

## 阶段 4：提高控制与仿真可信度

工作项：

- Smoother 使用真实 dt、独立前后轮 rate、accel/decel/jerk。
- 增加纵向控制与 vehicle feedback；处理 gear transition 和停稳换挡。
- 为 reverse 建独立 tracking 规则。
- 仿真增加 kinematic bicycle plant，订阅 ControlCommand/SCU command。
- 输出 tracking error、cross-track error、heading error、command saturation、stop reason 等诊断。

完成定义：SIL 中完成路线、倒车、停车和故障注入；指标达到预先定义的最大横向误差、最大停车距离和命令延迟。

## 阶段 5：完成算法能力而不是保留同名 fallback

优先级建议：

1. `obstacle_aware`：先定义输入合同和安全停车。
2. `frenet_lite`：采样、代价、边界和碰撞检查。
3. `hybrid_astar_parking`：车辆状态空间、解析扩展/数值扩展和换挡代价。
4. `mpc_sampler`：沿 horizon 多点代价、速度 rollout、控制变化率和约束。

在真实实现完成前，factory/status 必须显式标记 `experimental_fallback`，不能让名称暗示生产能力。

## 阶段 6：配置、文档与发布治理

工作项：

- 删除无效参数或实现它们；生成参数参考表。
- 合并重复 config/template，修复 smoke 默认路径。
- 归档历史 audit/report，建立 canonical docs。
- 增加 LICENSE、维护者、版本、changelog、release note、rollback procedure。
- 定义 sim、bench、vehicle 三套 launch/profile；vehicle profile 默认必须最保守。

完成定义：一个新开发者可在干净 ROS2 Humble 环境按一份文档完成构建、仿真和测试；配置中不存在 silent no-op key。

## 推荐优先队列

| 顺序 | 工作 | 预期收益 |
|---:|---|---|
| 1 | emergency trajectory 传播 | 关闭直接安全漏洞 |
| 2 | chassis watchdog | 关闭 publisher 消失后的旧命令风险 |
| 3 | control enable/estop 状态机 | 统一所有安全输入 |
| 4 | C++ gtest + ROS2 integration test | 防止修复再次回归 |
| 5 | 参数校验与配置清理 | 消除 silent misconfiguration |
| 6 | loader hardening | 提高数据完整性和安全性 |
| 7 | closed-loop simulation | 让控制算法验证有意义 |
| 8 | 高级规划算法 | 在基础安全和测试可信后扩展能力 |
