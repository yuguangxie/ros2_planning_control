# 审计索引

## 目标
为 `docs/audits/` 下全部审计报告提供中文导航索引。

## 状态
通过。

## 报告列表
- `AUDIT_SUMMARY.md`：总体结论、主要 P0/P1 问题、离线命令结果、跳过的 ROS2 命令。
- `PROJECT_STRUCTURE_AUDIT.md`：包边界和仓库结构审计。
- `AD_PACKAGE_COMPATIBILITY_AUDIT.md`：Low Speed Roadnet AD Package v1.1 路径和字段兼容性审计。
- `ROADNET_LOADER_AUDIT.md`：C++ loader 的解析、validation、checksum 和鲁棒性审计。
- `PLANNING_MODULE_AUDIT.md`：全局规划、运动规划、速度规划和规划节点集成审计。
- `CONTROL_MODULE_AUDIT.md`：控制器、车辆模型、安全、限幅/平滑和控制节点集成审计。
- `INTERFACES_TOPICS_AUDIT.md`：msg/srv 完整性和话题可配置性审计。
- `CONFIG_LAUNCH_AUDIT.md`：YAML 默认值、launch 文件和 bringup 可用性审计。
- `TESTING_WITHOUT_ROS2_AUDIT.md`：离线脚本和命令结果审计。
- `RISK_REGISTER.md`：优先级风险登记表。
- `FIX_PLAN.md`：分阶段修复计划、验收标准和建议 prompt。

## 证据
- 所有报告位于 `docs/audits/`。
- 审计依据包括 `git status --short`、`git ls-files`、递归文件清单、`rg` 扫描、带行号源码读取和离线脚本运行结果。

## 发现
### F-IDX-001：审计报告集合完整
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：为后续把生成工作区推进到可运行状态提供明确路线。
- 推荐修复：每完成一个修复阶段后同步更新索引和相关审计状态。
- 验证方法：检查本索引列出的所有文件均存在。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
