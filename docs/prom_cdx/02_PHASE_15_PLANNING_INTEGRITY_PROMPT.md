# Phase 15 Planning 完整性与正确性优化 Prompt

以下内容可直接复制为下一阶段任务：

````markdown
你现在位于 ROS2 低速自动驾驶项目根目录。

本任务是 Phase 15：在完全不修改 `src/yunle_chassis` 的前提下，完成 Planning 的 AD Package 安全、图搜索正确性、semantic/trajectory helper 可测试化和 ROS2 回归闭环。

项目已明确采用底盘硬件 watchdog：连续 500 ms 未收到 CAN 0x121 时由底盘硬件停车。本阶段不实现、不测试、不修改 Chassis 软件 watchdog。

## 一、必须先阅读

完整阅读：

- 根目录 `AGENTS.md`
- `docs/audit_cdx/00_AUDIT_INDEX.md`
- `docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `docs/audit_cdx/05_OPTIMIZATION_ROADMAP.md`
- `docs/prom_cdx/00_INDEX.md`
- `docs/prom_cdx/01_HARDWARE_WATCHDOG_AND_WORK_ROADMAP.md`
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/post_phase_14_recommendations.md`（若存在）
- `reports/final_generation_report.md`
- `docs/03_ros2_interfaces.md`
- `docs/04_planning_module_design.md`
- `docs/01_ad_package_contract.md`
- `docs/11_ad_package_to_algorithm_usage.md`
- `docs/PLANNING_OUTPUT_DATA_CONTRACT.md`
- `docs/SEMANTIC_GOAL_PLANNING_DESIGN.md`
- `docs/TRAJECTORY_CONTINUITY_AND_FULL_REFERENCE_PATH_DESIGN.md`
- `docs/REVERSE_PLANNING_POLICY.md`
- `src/low_speed_av_planning/CMakeLists.txt`
- `src/low_speed_av_planning/package.xml`
- `src/low_speed_av_planning/include/low_speed_av_planning/**`
- `src/low_speed_av_planning/src/**`
- `src/low_speed_av_planning/test/**`
- `src/low_speed_av_planning/config/planning_params.yaml`
- `src/low_speed_av_bringup/config/planning_params.yaml`
- `src/low_speed_av_bringup/test/**`
- `.github/workflows/ros2_humble_ci.yml`
- `scripts/run_offline_checks.py`
- `scripts/offline_repository_hygiene.py`

文件不存在时记录为缺失，不得假定存在。

## 二、当前已知状态

必须按当前源码重新核验：

- canonical AD Package 使用 `project_manifest.json`、`trajectory/waypoints.yaml`、`validation/validation_report.json`。
- Planning 已有 production library，`test_roadnet_loader` 和 `test_planning_algorithms` 直接链接该 library。
- Phase 14 sanitizer job 已实际执行 19 个 Planning gtest cases并通过；现有覆盖不包含完整 semantic helper、equal-cost determinism、负 cost、path containment 和 no-go/speed-zone 矩阵。
- 现有 ROS2 launch test主要覆盖 invalid goal -> emergency trajectory -> Control/SCU brake，Planning ready、PlanRoute/PlanMission success 等未执行。
- `CDX-P1-005` manifest path escape 仍为 OPEN。
- `CDX-P2-003` loader 结构校验、`CDX-P2-004` A* 正确性、`CDX-P2-005` semantic route summary、`CDX-P2-006` 回环 progress 仍需处理。
- 当前 GitHub workflow 总体为 `EXECUTED_FAIL`：sanitizer job PASS；build-test 在 `offline_repository_hygiene.py` 的 `git ls-files` 返回 128 后失败。
- `src/yunle_chassis` 必须保持零 diff。

## 三、目标

1. 所有 manifest 引用路径都被限制在 AD Package 根目录内；
2. 损坏或恶意 AD Package 一律 fail closed；
3. Dijkstra/A* 在当前合同下正确、可重复、可解释；
4. semantic anchor、terminal segment 和 trajectory construction 从 Node private 逻辑中提取为 production helper；
5. route summary、full reference path 和 local trajectory 的长度、s、终点和连续性一致；
6. Planning production C++ tests覆盖完整正负矩阵；
7. Planning service/status/trajectory 的 ROS2 integration tests实际可运行；
8. 修复 CI 主 job 的最小阻塞并保存同提交证据；
9. 不改变 Planning/Control/Interfaces/Bringup/Simulation/Chassis 职责边界。

## 四、允许修改范围

- `src/low_speed_av_planning/**`
- `src/low_speed_av_bringup/test/**`
- 必要的 Planning test-only fixture/helper/config
- Planning 和 Bringup 的 `CMakeLists.txt`、`package.xml`
- `scripts/offline_repository_hygiene.py` 和 `.github/workflows/ros2_humble_ci.yml`，仅限修复当前 CI 验证阻塞
- Planning 相关 README 和 docs
- `docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `reports/phase_14_report.md`
- `reports/final_generation_report.md`
- 新增 `reports/phase_15_report.md`

## 五、禁止修改范围

- 严禁修改 `src/yunle_chassis/**`。
- 不修改 Control 生产算法、安全状态机、SCU mapper 或默认输出合同。
- 不修改已有自定义 msg/srv 字段；新增接口前先核对 `docs/03_ros2_interfaces.md`，本阶段原则上不新增。
- 不修改正式 Roadnet 包、bringup sample 或 template sample 内容来让测试通过。
- 不引入旧 `manifest.json`、`trajectory/waypoints.json` 或根目录 `validation_report.json`。
- 不用 Nav2/Lanelet2 替代自定义 Roadnet。
- 不实现 Frenet、Hybrid A*、障碍物系统、闭环仿真或高级算法。
- 不把 Python replica、token search 或未执行测试写成 production C++ PASS。

## 六、开始前基线

执行并记录：

- `git status --short`
- `git diff --stat`
- `git diff --check`
- 当前 SHA、branch、remote tracking
- `ros2`、`colcon`、`cmake`、C++ compiler、pytest、uv、Python 可用性
- build/install/log/cache 是否被提交或误生成
- 当前 Planning production sources、library/executable/test target 关系
- 当前 RoadnetLoader path resolution/checksum/schema/validation 路径
- 当前 Dijkstra/A* cost、heuristic、tie-break 和 reverse/blocked 逻辑
- 当前 semantic anchor、route builder、full/local trajectory 和 progress crop 逻辑
- 当前 Planning C++ test case和 launch test注册数
- 当前 GitHub Actions run 和失败日志

运行修改前可行的 offline runner；ROS2 可用时运行定向 Planning build/test。

## 七、生产实现要求

### 7.1 Manifest path containment

- canonicalize package root 和 resolved target。
- 拒绝 absolute path、`..` escape、mixed separator escape 和 symlink escape。
- manifest.files、manifest.hashes、checksums 中用于实际读取的路径必须使用同一 containment helper。
- 不允许“checksum 检查安全，但真正 LoadFile 路径可逃逸”。
- Windows 和 Linux path 语义都应有测试；symlink case 在不支持的平台可显式 SKIP。

### 7.2 Loader 结构和数值校验

至少拒绝：

- 重复 node/edge/path/waypoint/semantic ID；
- edge from/to 引用不存在 node；
- 非有限或负 cost/length/speed；
- waypoint x/y/yaw/kappa/s/v 非有限；
- waypoint index start/end/count 不一致；
- index range 越界、重叠或与 edge_id 不一致；
- manifest hashes 和 checksums 冲突；
- validation failed 或 blocking_errors > 0；
- 不兼容 schema/version。

所有错误必须包含可操作的文件、ID 或字段原因。

### 7.3 Dijkstra/A* 正确性和确定性

- 明确 edge cost 合同和是否允许零 cost；负 cost必须由 loader/planner fail closed。
- A* heuristic 必须 admissible，或明确实现 weighted A* 并在状态/文档中说明非最优语义。
- 默认配置下 A* 与 Dijkstra 在 admissible 场景得到相同最优 cost。
- 等价 cost 使用 node/edge ID 或明确序号 tie-break，结果可重复。
- start==goal、unreachable、disabled、blocked、reverse-disabled 都有确定结果。
- 不借本阶段实现新的高级 planner。

### 7.4 Semantic/trajectory production helper

将仍深埋 PlanningNode private 的纯逻辑提取到 production helper/library，Node 和测试调用同一实现。至少覆盖：

- current pose anchor；
- task/parking/charging linked_node；
- linked_edge fallback；
- null、`"null"`、none 不作为 node ID；
- same edge forward；
- goal behind 且 reverse disabled；
- final semantic stop；
- terminal local segment；
- route length/time 重算；
- route_s 单调；
- full/local trajectory 终点一致；
- max point jump 和 continuity。

### 7.5 Progress/crop

- 不再无约束全局最近点搜索导致回环跳进度。
- 使用显式 progress state/search window，并结合 heading、gear、trajectory identity。
- 新 trajectory、reload、algorithm switch 时明确 reset。
- 保持低速保守默认值。

## 八、C++ 测试要求

继续使用 `ament_cmake_gtest` 并直接链接 production target。不得复制算法。

RoadnetLoader 至少覆盖：canonical、1.1.x、validation failure、blocking error、checksum/hash、两种 end index、tampered waypoint/index、duplicate IDs、negative/non-finite cost、absolute/relative/symlink escape。

Graph/planner 至少覆盖：shortest path、unreachable、blocked、disabled、reverse、start==goal、equal-cost determinism、A*/Dijkstra cost equivalence、negative cost rejection。

Motion/speed 至少覆盖：multi-edge stitch、boundary dedup、route_s、horizon crop、stop-and-wait、curvature bounds、speed/no-go area、obstacle stub current contract。

Semantic/helper 至少覆盖第 7.4 节全部行为。

坏 fixture 必须在临时目录生成，不修改 sample或正式包。随机测试固定 seed并打印 seed。

## 九、ROS2 integration test

使用 launch_testing，至少覆盖：

1. canonical sample加载并发布 ready RoadnetStatus；
2. PlanRoute success，收到 GlobalRoute 和 Trajectory；
3. PlanMission task/parking/charging success；
4. invalid goal发布 failure status和 emergency trajectory；
5. reload invalid package保持 fail closed；
6. late subscriber获取必要 static/status；
7. QoS、republish和timeout有确定断言；
8. process exit有 bounded wait和诊断。

本阶段 integration test不启动 Chassis Driver、不连接真实 UDP。

## 十、CI 和执行规则

### ROS2 可用

运行定向后再全量：

```bash
colcon build --symlink-install --packages-up-to low_speed_av_planning low_speed_av_bringup
colcon test --packages-select low_speed_av_planning low_speed_av_bringup --event-handlers console_direct+
colcon test-result --verbose
colcon build --symlink-install
colcon test --event-handlers console_direct+
colcon test-result --verbose
python3 scripts/run_offline_checks.py
```

### ROS2 不可用

- 运行 offline runner、validator、template/repository hygiene、JSON/UTF-8/Markdown link、CMake/package consistency和 `git diff --check`。
- C++ 标记 `GENERATED_NOT_EXECUTED`。
- ROS2 标记 `SKIPPED_ROS2_UNAVAILABLE`。

### CI

- 最小修复当前 `git ls-files` exit 128；输出子进程 stderr。
- 明确 container safe-directory和工作目录。
- full build-test、sanitizer、Planning launch tests均需执行。
- 上传 test-result/log artifacts。
- workflow任一 required job失败时状态为 `EXECUTED_FAIL`。

## 十一、文档和报告

更新 Planning README、AD Package/Planning design、测试矩阵、audit findings、Phase 14/final report，并新增 `reports/phase_15_report.md`。

报告必须包含：目标、baseline、files changed、设计决策、AD Package兼容、target/test映射、实际执行数、CI URL/SHA、findings、限制和下一阶段交接。

## 十二、Finding 状态规则

- `CDX-P1-005` 只有所有实际读取路径 containment 完成且负例实际通过才可 FIXED。
- `CDX-P1-006` 按实际 production C++覆盖和执行证据判断。
- `CDX-P1-007` 只有同提交 full CI PASS 才可 FIXED。
- `CDX-P2-003/004/005/006` 分别依据生产代码和测试判断，不得批量预设 FIXED。
- `CDX-P0-002` 保持 `OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`，本阶段不修改 Chassis。

## 十三、完成标准

- manifest path无法逃逸根目录；
- 损坏/恶意包fail closed；
- A*/Dijkstra合同明确且结果确定；
- semantic/trajectory helper进入production target并有直接测试；
- Planning service/status/trajectory integration source完整；
- 当前环境可运行检查通过；
- CI/full ROS2结果准确；
- `git diff --check` PASS；
- 正式 fixture未修改；
- `git diff -- src/yunle_chassis` 为空；
- 未进入高级算法和闭环仿真。

## 十四、最终回复

只汇报实际生产修改、测试target、实际PASS/FAIL/SKIPPED、CI状态、finding状态、已知限制和Phase 16交接。完成后停止，不自动实施Control阶段。
````
