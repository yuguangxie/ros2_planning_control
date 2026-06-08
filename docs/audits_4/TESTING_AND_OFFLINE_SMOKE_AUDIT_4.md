# 测试与离线 Smoke 审计 4

## Objective（目标）
审计无 ROS2 环境下可执行的脚本是否覆盖 SCU mapper、LQR、Ackermann、安全和 AD Package 基础行为，并记录解释器可用性。

## Status（状态）
Partial。`uv` 与 FreeCAD Python 均可用于离线验证；默认 WindowsApps `python` 和 `py` 不可用。`uv run python` 本身缺少 PyYAML，但 `uv run --with pyyaml python` 可跑通依赖 YAML 的脚本。离线脚本仍不能替代 C++ 编译、ROS2 topic 或底盘驱动验证。

## Evidence（证据）
- `python scripts\validate_expected_tree.py`：exit 1，stdout 为空。
- `py scripts\validate_expected_tree.py`：命令不存在。
- `uv run python --version`：Python 3.14.3。
- `uv run python scripts\validate_expected_tree.py`：`Expected tree OK: .`。
- `uv run python scripts\offline_scu_lqr_smoke.py`：`Offline SCU/LQR smoke OK`。
- `uv run python` 执行依赖 YAML 的脚本时失败，原因是缺少 PyYAML。
- `uv run --with pyyaml python scripts\validate_sample_ad_package.py`：`AD Package OK: ...`。
- `uv run --with pyyaml python scripts\offline_algorithm_smoke.py`：route、trajectory、pp、stanley、ackermann、estop 通过。
- `uv run --with pyyaml python scripts\offline_remaining_fixes_smoke.py`：checksum/bad_validation/bad_index/semantics/LQR/MPC/estop 通过。
- FreeCAD Python `validate_expected_tree.py`：`Expected tree OK: .`。
- FreeCAD Python `validate_sample_ad_package.py`：`AD Package OK: ...`。
- FreeCAD Python `offline_algorithm_smoke.py`：route、trajectory、pp、stanley、ackermann、estop 通过。
- FreeCAD Python `offline_remaining_fixes_smoke.py`：checksum/bad_validation/bad_index/semantics/LQR/MPC/estop 通过。
- FreeCAD Python `offline_scu_lqr_smoke.py`：`Offline SCU/LQR smoke OK`。
- SCU/LQR 静态检查：`scripts/offline_scu_lqr_smoke.py:231` 到 `scripts/offline_scu_lqr_smoke.py:239`。

## Findings（发现）
| ID | Severity | Status | Finding |
|---|---|---|---|
| A4-TEST-001 | P3 | Pass | 新增 `offline_scu_lqr_smoke.py` 覆盖 SCU mapping、Ackermann、LQR 配置敏感和有限输出。 |
| A4-TEST-002 | P3 | Pass | 原有离线脚本仍通过，未破坏 AD Package 和规划/控制基础 smoke。 |
| A4-TEST-003 | P2 | Partial | 测试是 Python 镜像和静态源码检查，未直接编译/执行 C++ `ScuCommandMapper` 或 `LqrController`。 |
| A4-TEST-004 | P3 | Partial | 当前机器默认 `python` 不可用；可使用 `uv run --with pyyaml python ...` 或 FreeCAD Python。 |

## Impact on planning/control/chassis operation（对规划、控制和底盘运行的影响）
离线 smoke 能快速发现映射和算法明显错误，但不能证明 ROS2 消息类型、C++ ABI、CMake 依赖或底盘驱动接收行为正确。

## Recommended fix（推荐修复）
- 添加 C++ gtest 或 CLI smoke target 直接链接 control library。
- 添加 PowerShell wrapper 自动优先使用 `uv run --with pyyaml python`，再 fallback 到 conda/FreeCAD Python。
- 在 ROS2 CI 中运行 `colcon test`。

## Verification method（验证方法）
- 已运行全部可用离线脚本并通过。
- 已记录 `python`/`py` 失败原因，并补充 `uv` 运行结果。
- 未运行 C++/ROS2 测试。

## ROS2 commands skipped due to unavailable environment
SKIPPED_ROS2_UNAVAILABLE:
- `colcon test`
- `colcon test-result --verbose`
