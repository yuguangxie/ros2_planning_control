# AD Package 兼容性审计

## 目标
审计项目对 Low Speed Roadnet AD Package v1.1 规范路径、manifest、validation、waypoint 字段映射和索引格式的兼容性。

## 状态
部分通过。

## 证据
- loader 要求 `project_manifest.json`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:64`。
- schema 校验见 `src/low_speed_av_planning/src/roadnet_loader.cpp:70`，`1.1.x` 支持见 `src/low_speed_av_planning/src/roadnet_loader.cpp:73` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:75`。
- manifest `files` 读取见 `src/low_speed_av_planning/src/roadnet_loader.cpp:80` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:83`。
- canonical fallback 见 `src/low_speed_av_planning/src/roadnet_loader.cpp:179` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:183`。
- `kappa` 映射到 `kappa_1pm`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:142`。
- `v_mps` 映射到 `target_speed_mps`，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:143`。
- `end_index_exclusive` 优先、legacy inclusive `end_index` fallback 见 `src/low_speed_av_planning/src/roadnet_loader.cpp:161` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:166`。
- 离线 validator 拒绝旧路径，见 `scripts/validate_sample_ad_package.py:61` 至 `scripts/validate_sample_ad_package.py:64`。

## 发现
### F-AD-001：canonical 入口路径正确
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：实现方向符合 v1.1 包结构，避免旧 `manifest.json` 主路径假设。
- 推荐修复：继续保持旧路径不作为 primary input。
- 验证方法：静态检查和 `validate_sample_ad_package.py`。

### F-AD-002：C++ RoadnetLoader 未加载 semantics 文件
- 严重级别：P2
- 状态：失败
- 对规划/控制/车辆运行影响：speed zone、task point、parking point、charging point、no-go area 等语义无法影响规划。
- 推荐修复：在 `RoadnetPackage` 中增加 typed semantics 容器，通过 manifest files 解析 `areas`、`route_points`、`task_points`、`parking_points`、`charging_points`。
- 验证方法：使用 sample semantics 编写 loader 单元测试，并覆盖缺少可选 semantics 文件时的 warning。

### F-AD-003：manifest validation 未被独立检查
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：如果 manifest 中 validation failed，而 validation report 字符串检查未命中，包可能被错误加载。
- 推荐修复：结构化解析 `manifest_node["validation"]["status"]` 和 `manifest_node["validation"]["blocking_errors"]`，并在加载 topology/waypoints 前拒绝失败包。
- 验证方法：构造 manifest validation failed、validation report passed 的负样例。

### F-AD-004：C++ runtime 未完成 checksum/hash 校验
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：ROS2 规划节点运行时无法确认 roadnet 文件完整性。
- 推荐修复：实现 `checksums.sha256` 和 `manifest.hashes` 的 SHA-256 校验，并由 `roadnet.verify_checksums` 控制失败/警告策略。
- 验证方法：篡改 `trajectory/waypoints.yaml` 后确认 loader 在校验开启时拒绝加载。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `ros2 service call /planning/reload_roadnet ...`
