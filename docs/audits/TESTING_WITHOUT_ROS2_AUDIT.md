# 无 ROS2 测试审计

## 目标
审计无 ROS2 环境下的离线验证脚本、运行结果、覆盖范围和缺口。

## 状态
部分通过。

## 证据
- expected tree validator 的必需文件列表见 `scripts/validate_expected_tree.py:7` 至 `scripts/validate_expected_tree.py:47`。
- sample AD package validator 的 canonical 文件清单见 `scripts/validate_sample_ad_package.py:12` 至 `scripts/validate_sample_ad_package.py:28`。
- validator 拒绝旧路径，见 `scripts/validate_sample_ad_package.py:61` 至 `scripts/validate_sample_ad_package.py:64`。
- validator 校验 checksum，见 `scripts/validate_sample_ad_package.py:116` 至 `scripts/validate_sample_ad_package.py:129`。
- offline smoke 加载 manifest/topology/waypoints/index，见 `scripts/offline_algorithm_smoke.py:98` 至 `scripts/offline_algorithm_smoke.py:101`。
- offline smoke 验证路线、轨迹和有限控制输出，见 `scripts/offline_algorithm_smoke.py:102` 至 `scripts/offline_algorithm_smoke.py:112`。

## 命令结果
- `python scripts\validate_expected_tree.py`：失败，Windows Store Python 占位程序退出码 1，无 stdout。
- `python scripts\validate_sample_ad_package.py`：失败，Windows Store Python 占位程序退出码 1，无 stdout。
- `python scripts\offline_algorithm_smoke.py`：失败，Windows Store Python 占位程序退出码 1，无 stdout。
- `py scripts\*.py`：失败，`py` 命令不存在。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`：通过。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`：通过。

## 发现
### F-TW-001：离线脚本存在且有意义
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：能捕捉结构缺失、sample package 损坏、路线拼接退化和控制输出非有限值。
- 推荐修复：在无 ROS2 CI 或 Windows 本地流程中保留这三条脚本。
- 验证方法：使用真实 Python 解释器运行三条脚本。

### F-TW-002：offline smoke 复刻逻辑，没有测试生成的 C++ 代码
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：Python smoke 通过并不代表 C++ RoadnetLoader、Dijkstra/A*、控制器可以编译或行为一致。
- 推荐修复：增加不依赖 ROS graph runtime 的 C++ 单元测试，或增加链接规划/控制库的小型 CLI smoke target。
- 验证方法：对 C++ algorithm classes 运行原生单元测试。

### F-TW-003：Windows client 中 Python 可用性不稳定
- 严重级别：P3
- 状态：部分通过
- 对规划/控制/车辆运行影响：开发者可能因为 Windows Store Python 占位程序看到误失败。
- 推荐修复：文档化 Python 解释器要求，或增加 PowerShell wrapper 自动寻找可用 Python。
- 验证方法：在当前 Windows client 上运行 wrapper。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
- SKIPPED_ROS2_UNAVAILABLE: `ros2 test`
