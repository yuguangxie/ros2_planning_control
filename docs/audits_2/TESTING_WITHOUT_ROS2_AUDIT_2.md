# 无 ROS2 测试第二轮审计

## Objective
审计当前 Windows 无 ROS2 环境下可运行的离线脚本、脚本覆盖范围、解释器可用性，以及仍缺失的 C++/ROS2 验证。

## Status: Partial
三条离线脚本通过，但它们主要验证文件树、sample AD Package 和 Python 版算法烟测，不等价于 C++ 编译或 ROS2 节点运行验证。

## Evidence
- `python scripts\validate_expected_tree.py`：退出码 1，stdout 为空。
- `py scripts\validate_expected_tree.py`：PowerShell 报 `The term 'py' is not recognized...`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`：通过，输出 `Expected tree OK: .`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`：通过，输出 `AD Package OK: src\low_speed_av_bringup\sample_ad_package (3 nodes, 2 edges, 6 waypoints)`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`：通过，输出 `Offline algorithm smoke OK: route=['E_L001_F', 'E_L002_F'], traj_points=6, pp=(0.500,0.000), stanley=(0.500,0.000), ackermann=finite, estop=ok`。
- sample validator 执行 checksum 比对逻辑，见 `scripts/validate_sample_ad_package.py:116` 至 `scripts/validate_sample_ad_package.py:129`。
- offline smoke 覆盖 route、trajectory、Pure Pursuit、Stanley、Ackermann 和 estop。

## Findings
### A2-TST-001：离线脚本可用且通过
- Severity: P3
- Status: Pass
- Impact on planning/control/vehicle operation: 基础文件合同和样例算法链路没有明显回归。
- Recommended fix: 在 README 或审计报告中记录可用解释器路径。
- Verification method: 用 FreeCAD Python 重跑三条脚本。

### A2-TST-002：默认 `python` 和 `py` 不可用
- Severity: P3
- Status: Partial
- Impact on planning/control/vehicle operation: 新开发者在 Windows 上可能以为脚本失败是工程问题。
- Recommended fix: 文档写明 Windows 解释器兜底；或提供 `scripts/run_offline_checks.ps1` 自动探测解释器。
- Verification method: 运行包装脚本并输出解释器选择。

### A2-TST-003：Python smoke 不是 C++ runtime 测试
- Severity: P2
- Status: Partial
- Impact on planning/control/vehicle operation: C++ 编译错误、ROS2 参数、接口生成、QoS 和节点时序问题无法被发现。
- Recommended fix: 增加不依赖 ROS graph 的 C++ CLI smoke target 或 gtest。
- Verification method: 有编译器/ROS2 依赖时运行 C++ tests。

### A2-TST-004：缺少 Loader 负样例测试
- Severity: P2
- Status: Partial
- Impact on planning/control/vehicle operation: failed validation、bad index、checksum mismatch 的拒载行为缺少自动回归保护。
- Recommended fix: 增加临时目录生成坏包的 Python/C++ 测试。
- Verification method: 每类坏包都应失败并输出明确错误。

## ROS2 commands skipped due to unavailable environment
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test-result --verbose`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 launch low_speed_av_bringup planning_control_demo.launch.py`

