# 项目结构审计

## 目标
确认项目是否生成了要求的四个 ROS2 包，并保持接口、规划、控制、bringup 的职责边界。

## 状态
部分通过。

## 证据
- `src/low_speed_av_interfaces/CMakeLists.txt:8` 至 `src/low_speed_av_interfaces/CMakeLists.txt:20` 只生成 msg/srv 接口。
- `src/low_speed_av_planning/CMakeLists.txt:15` 至 `src/low_speed_av_planning/CMakeLists.txt:29` 只编译规划相关源文件。
- `src/low_speed_av_control/CMakeLists.txt:14` 至 `src/low_speed_av_control/CMakeLists.txt:27` 只编译控制相关源文件。
- `src/low_speed_av_bringup/CMakeLists.txt:6` 安装 `launch`、`config`、`sample_ad_package`。
- `scripts/validate_expected_tree.py:7` 至 `scripts/validate_expected_tree.py:47` 检查关键生成文件。
- `git status --short` 显示所有文件未跟踪；`git ls-files` 无输出。

## 发现
### F-PS-001：四包结构存在
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：包边界清晰，满足后续 ROS2 工作空间组织要求。
- 推荐修复：无需针对目录结构修复。
- 验证方法：运行 `scripts/validate_expected_tree.py` 并检查 `Get-ChildItem -Recurse`。

### F-PS-002：仓库没有可用的跟踪基线
- 严重级别：P2
- 状态：失败
- 对规划/控制/车辆运行影响：后续修复阶段难以用 git 区分生成内容、审计内容和用户改动，回归控制较弱。
- 推荐修复：在继续实现前建立一次基线提交，或明确记录该工作区为未跟踪生成产物。
- 验证方法：`git status --short`、`git ls-files`。

### F-PS-003：包结构完整，但 ROS2 节点运行行为不足
- 严重级别：P1
- 状态：部分通过
- 对规划/控制/车辆运行影响：源码分层存在，但规划节点和控制节点还不能形成完整运行闭环。
- 推荐修复：保持 package 边界不变，在各自节点内补充服务、订阅、发布与算法调用链路。
- 验证方法：静态审计加 ROS2 环境中的集成测试。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon build`
- SKIPPED_ROS2_UNAVAILABLE: `colcon test`
