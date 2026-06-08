# 无 ROS2 测试审计 3

## Objective（目标）
审计 Windows 无 ROS2 环境下的离线测试、脚本覆盖范围、Python 解释器发现、checksum 负例、semantics 测试和 C++/CLI smoke 准备状态。

## Status（状态）
Partial。离线 Python 脚本有意义且通过，但默认 `python` 和 `py` 不可用；缺少实际编译运行的 C++ gtest 或 CLI smoke target。

## Evidence（证据）
- expected tree 脚本存在并通过：`scripts/validate_expected_tree.py`。
- sample validator canonical required list：`scripts/validate_sample_ad_package.py:12`。
- sample validator 禁止旧路径：`scripts/validate_sample_ad_package.py:62` 到 `scripts/validate_sample_ad_package.py:64`。
- sample validator checksum 检查：`scripts/validate_sample_ad_package.py:116` 到 `scripts/validate_sample_ad_package.py:129`。
- offline algorithm smoke 覆盖 route、trajectory、controller、ackermann、estop：`scripts/offline_algorithm_smoke.py`。
- remaining fixes smoke 覆盖 checksum/bad validation/bad index/semantics/LQR/MPC/estop：`scripts/offline_remaining_fixes_smoke.py:211` 到 `scripts/offline_remaining_fixes_smoke.py:230`。
- `python scripts\validate_expected_tree.py` 失败，exit 1，stdout 为空。
- `py scripts\validate_expected_tree.py` 失败，`py : The term 'py' is not recognized...`。
- `C:\Program Files\FreeCAD 1.2\bin\python.exe` 可运行全部 Python 检查并通过。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A3-TN-001 | P3 | Pass | Python 离线脚本覆盖结构、样例包、算法、checksum 负例和语义约束。 |
| A3-TN-002 | P2 | Partial | 离线脚本多为 Python 镜像和源码模式检查，不等于 C++ 编译执行。 |
| A3-TN-003 | P3 | Partial | 当前机器默认 `python` 不可用，`py` 不存在，需要使用 FreeCAD Python。 |
| A3-TN-004 | P2 | Still Open | 未发现 C++ gtest 或 CLI smoke target。 |

## Impact on planning/control/vehicle operation（对规划、控制和车辆运行的影响）
离线脚本能较快发现 AD Package 结构、checksum、规划逻辑和控制公式错误。缺少 C++ 编译级测试意味着模板、include、链接、ROSIDL 字段名和真实 C++ 分支仍可能在构建时暴露问题。

## Recommended fix（推荐修复）
- 新增 `test_roadnet_loader.cpp`、`test_planning_core.cpp`、`test_control_core.cpp` 或一个 CLI smoke executable。
- 添加 `scripts/run_offline_checks.ps1` 自动寻找可用 Python，并清晰打印 interpreter。
- 在 README 写明默认 Windows Store Python placeholder 的处理方式。

## Verification method（验证方法）
- 已执行并通过：
  - `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_expected_tree.py`
  - `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\validate_sample_ad_package.py`
  - `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_algorithm_smoke.py`
  - `C:\Program Files\FreeCAD 1.2\bin\python.exe scripts\offline_remaining_fixes_smoke.py`
- 已记录 `python` 和 `py` 不可用。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon test`
- `colcon test-result --verbose`

