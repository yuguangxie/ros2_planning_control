# 审计范围、文件盘点与证据

## 遍历方法

本轮从仓库根目录执行文件枚举，排除 `.git/build/install/log`。对源码、配置和文档做了以下检查：

- 全文件路径枚举、扩展名统计、目录统计、最大文件和源码行数统计；
- CMake/package.xml、全部 msg/srv、全部 Planning/Control/Simulation/Chassis 源文件静态阅读；
- README、核心设计文档、操作文档、历史 audit/report 的标题和关键结论扫描；
- 所有 JSON 解析、所有文本类文件严格 UTF-8 解码、本地 Markdown 链接检查；
- sample、两套正式 Roadnet、template 和 checksum 重复/漂移检查；
- 全部根目录 offline smoke 的执行或显式失败记录；
- 当前 Git 状态和 ROS2/Python 环境检查。

## 文件统计

| 类型 | 数量 |
|---|---:|
| 总文件 | 447 |
| Markdown | 200 |
| JSON | 74 |
| C++ header | 40 |
| C++ source | 38 |
| YAML | 22 |
| Python | 21 |
| ROS msg | 18 |
| ROS srv | 5 |
| XML | 7 |
| 其他 | CSV 4、SHA256 4、SVG 2、RViz/DBC/DOCX/PS1 各 1 等 |

Markdown 约 1.5 万行，其中历史 audit 77 份、reports 41 份、普通 docs 约 49 份。代码规模约为：Planning 3130 行、Control 1457 行、Simulation 1346 行、Chassis Driver 1751 行。

## 受审目录

| 目录 | 审计内容 |
|---|---|
| `src/low_speed_av_interfaces` | CMake、package、7 msg、5 srv、README |
| `src/low_speed_av_planning` | 33 个 C++ header/source、config、launch、README、唯一 test wrapper |
| `src/low_speed_av_control` | 31 个 C++ header/source、config、launch、README |
| `src/low_speed_av_bringup` | launch、3 config、完整 sample AD Package、README |
| `src/low_speed_av_simulation` | 两个节点、config、launch、RViz、package metadata |
| `src/yunle_chassis` | interfaces、driver、DBC、协议 DOCX、README/说明 |
| `roadnet_ad_package_*_1/_2` | 两套 manifest、拓扑、waypoint、semantics、schema、validation、checksum |
| `scripts` | 1 个环境检查、10 个离线 smoke/validator、1 个可视化工具 |
| `templates` | expected tree、offline validators、sample config、sample AD Package |
| `docs` | 核心设计、操作、集成、历史 audits 1–5、chassis audit |
| `reports` | phase 00–11、优化/修复报告、Ubuntu runtime 报告、拓扑产物 |
| `prompts` / `skills` | 生成流程和技能约束，检查与当前代码/报告的一致性 |

## 数据文件结论

- canonical sample、正式包 `_1`、正式包 `_2` 都通过现有 Python checksum/结构 validator。
- 正式包 `_1` 为 20 nodes / 26 edges / 737 waypoints，manifest validation 为 warning。
- 正式包 `_2` 为 16 nodes / 22 edges / 496 waypoints，manifest validation 为 warning。
- bringup sample 与 template sample 有 23 个完全相同文件；这是复制关系，不是生成关系。
- 两套正式包的 7 个 schema 文件互相完全相同。
- 全仓库发现 30 组完全重复文件，共 60 个文件，主要来自 sample/template 与正式包共享 schema。

## 历史验证证据边界

可确认的历史事实：2026-06-10 的 Ubuntu ROS2 报告记录过 7 packages build/test 完成，但 `colcon test-result` 为 0 tests；报告也明确不建议连接真实底盘运动测试。

不能自动确认的内容：后续修复后的 `UBUNTU_ROS2_REVALIDATION_*` 多为步骤和期望，没有与当前 commit 绑定的完整终端输出。因此本审计将当前 ROS2 build/runtime 状态标为“历史上构建过，当前快照未重新验证”，而不是简单的 PASS 或 FAIL。

## 当前工作区状态

审计开始时 `git status --short` 为空。审计期间只新增 `docs/audit_cdx/` 和阶段审计报告，不修改源码、配置、Roadnet 数据或历史结论。

## 审计限制

- 当前 Windows 环境没有 ROS2、colcon 和本地 C++ 编译器，不能对当前 C++ 做编译/链接确认。
- Python 可执行环境没有 pytest，不能在本机按 ament pytest 方式执行唯一注册测试。
- 未连接 Yunle 底盘、CAN 网关或 HIL，无法验证物理协议周期、硬件 watchdog、转角方向和车辆响应。
- DOCX 协议文件作为仓库资产被纳入盘点；本轮主要依据已抽取到 DBC、代码和现有协议说明中的字段合同，没有重新做版面级 DOCX 校对。
- 结论是静态审计和离线 smoke 结果，不构成车辆安全认证。
