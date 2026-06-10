# Testing And Offline Smoke Audit

## Objective

审计当前无 ROS2 环境下的离线测试覆盖、执行结果和缺口。

## Scope

- `scripts/*.py`
- `scripts/check_ros2_env.ps1`
- 本次执行结果

## Status

Pass for available offline checks, Partial for ROS2 integration coverage.

## Evidence

已执行并通过：

```text
Expected tree OK: .
AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)
Offline algorithm smoke OK: route=['E_L001_F', 'E_L002_F'], traj_points=6, ...
Remaining fixes smoke OK: checksum/bad_validation/bad_index rejected, ...
Offline SCU/LQR smoke OK: SCU mapping safe, Ackermann finite, Riccati LQR finite/config-sensitive
Offline simulation smoke OK: nodes=16, edges=22, waypoints=496, ...
```

## Findings

| ID | Severity | Status | Finding | Impact | Recommended fix | Verification |
|---|---|---|---|---|---|---|
| AUD5-TEST-001 | P3 | Pass | 离线脚本覆盖 expected tree、AD Package、算法、SCU/LQR、simulation smoke。 | 无 ROS2 时仍可发现大量静态/数据问题。 | 保持 CI 运行。 | `uv run ...`。 |
| AUD5-TEST-002 | P2 | Partial | 离线脚本多为 Python replica，不等价于 C++ 编译和 ROS2 节点行为。 | 可能漏掉 C++/ROS2 集成错误。 | 增加 gtest 或 ROS2 launch tests。 | `colcon test`。 |
| AUD5-TEST-003 | P2 | Partial | 当前 Python 默认命令是 Windows Store 占位符，需要 `uv`。 | 新用户直接 `python` 可能失败。 | 文档中保留 `uv` 或设置 Python 环境。 | `Get-Command python,uv`。 |

## ROS2 Commands Run Or Skipped

Run:

- All offline commands in summary.

SKIPPED_ROS2_UNAVAILABLE:

- `colcon build --symlink-install`
- `colcon test`
- `ros2 launch ...`

## Remaining Uncertainty

ROS2 runtime tests、C++ ABI/linking、message generation 未覆盖。

