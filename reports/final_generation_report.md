# Final Generation Report
- Goal: 生成 Low Speed Roadnet AD Package v1.1 的 ROS2 planning/control workspace。
- Files changed: 新增四个包 `low_speed_av_interfaces`、`low_speed_av_planning`、`low_speed_av_control`、`low_speed_av_bringup`，新增 `scripts/` 离线检查、`docs/13_generated_workspace_usage.md` 和 phase reports。
- Key design decisions: planning/control 分离；算法使用显式 factory；ROS2 不可用时只运行 Python 离线检查。
- AD Package compatibility notes: loader 读取 `project_manifest.json`，支持 `schema=low_speed_roadnet_ad_package` 和 `1.1.x`；使用 `manifest.files`，fallback 到 canonical paths；支持 `end_index_exclusive` 和 legacy inclusive `end_index`；样例包不使用旧路径。
- Config/topic compatibility notes: 默认定位 `/localization/pose`，轨迹 `/planning/trajectory`，全局路线 `/planning/global_route`，控制命令 `/control/command`；所有默认话题都在 YAML 中可配置。
- Tests or offline checks run: `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/validate_expected_tree.py` -> OK；`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/validate_sample_ad_package.py` -> OK，3 nodes、2 edges、6 waypoints；`C:\Program Files\FreeCAD 1.2\bin\python.exe scripts/offline_algorithm_smoke.py` -> OK，route `E_L001_F,E_L002_F`，6 trajectory points，Pure Pursuit/Stanley finite commands。
- ROS2 commands skipped because ROS2 is unavailable: SKIPPED_ROS2_UNAVAILABLE: `source /opt/ros/<distro>/setup.bash`; SKIPPED_ROS2_UNAVAILABLE: `colcon build`; SKIPPED_ROS2_UNAVAILABLE: `colcon test`; SKIPPED_ROS2_UNAVAILABLE: `colcon test-result --verbose`; SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`。
- Known limitations: 当前机器没有 ROS2，未验证 CMake/IDL/节点链接；LQR、frenet_lite、hybrid_astar_parking 保持后续扩展骨架。
- Next phase handoff: 在 ROS2 环境运行 build/test/test-result，并补充节点发布订阅集成测试。

## 审计后两阶段优化状态

### Phase 13：端到端安全语义

- 状态：`PARTIALLY_COMPLETED`
- 当前提交：`44485cc007e6fd4d82d3e90edb0222a0fbb32867`
- `CDX-P0-001`：FIXED。Control 已消费 trajectory emergency/status 并在 controller 前 fail closed。
- `CDX-P0-002`：OPEN。Chassis Driver 仍缺少独立周期 command scheduler、command timeout、startup/shutdown stop 和 watchdog diagnostics。
- ROS2/C++ 执行：`SKIPPED_ROS2_UNAVAILABLE`；Phase 13 的 Python offline PASS 不等价于 ROS2/C++ 回归通过。
- 证据：`reports/phase_13_report.md`。

### Phase 14：自动化测试底座与 CI

- 状态：`IMPLEMENTED_WITH_ACCEPTED_PHASE_13_GAP`
- Gate override：依据 2026-07-11 明确授权继续实施，不再标记为 `BLOCKED_BY_PHASE_13`；该例外不接受或关闭 Chassis watchdog 缺口。
- 已完成：Planning/Control 注册 production-linked gtest；Chassis 建立 `chassis_driver_core` 并为当前 DBC、codec、0x121 frame path 注册 production-linked gtest；Bringup 注册 Planning -> Control -> SCU failure/brake launch test；Python 默认入口、空 parking fixture、统一 runner、template/config/sample 漂移检查和 ROS2 Humble CI 已配置。
- 实际执行：`scripts/run_offline_checks.py` 为 17 PASS / 0 FAIL / 0 SKIPPED；sample、正式 `_1`、正式 `_2` validator 均 PASS；仓库 UTF-8/JSON/Markdown link/hardware-isolation 检查 PASS；`git diff --check` PASS。
- C++/ROS2：48 个 C++ test source cases 与 ROS2 launch source 均未执行，状态为 `GENERATED_NOT_EXECUTED` / `SKIPPED_ROS2_UNAVAILABLE`，不得推断为 PASS。
- CI 状态：`CONFIGURED_NOT_EXECUTED`。workflow 已创建，但没有当前工作区提交的 GitHub Actions 执行证据。
- `CDX-P1-006`：`PARTIALLY_FIXED`；`CDX-P1-007`：`CONFIGURED_NOT_EXECUTED`；`CDX-P3-001/002/003`：`FIXED`。
- `CDX-P0-002`：`OPEN / ACCEPTED_PHASE_14_GAP`。startup/timeout/invalid replay/shutdown/diagnostics watchdog specs 明确跳过，不计为 PASS。
- 证据：`reports/phase_14_report.md`。
