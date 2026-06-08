# RoadnetLoader 审计

## 目标
评估 C++ RoadnetLoader 的正确性、鲁棒性和 AD Package v1.1 合同符合度。

## 状态
部分通过。

## 证据
- `json_string` 使用字符串搜索，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:26` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:40`。
- manifest 虽然用 YAML 加载，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:69`，但 schema/package_id 等仍用字符串搜索，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:70` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:79`。
- validation report 失败/阻塞错误检查是字符串搜索，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:93` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:99`。
- topology 使用 YAML parser 加载，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:102`。
- waypoint 字段加载和映射见 `src/low_speed_av_planning/src/roadnet_loader.cpp:131` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:154`。
- checksum 函数只写 warning，见 `src/low_speed_av_planning/src/roadnet_loader.cpp:186` 至 `src/low_speed_av_planning/src/roadnet_loader.cpp:192`。

## 发现
### F-RL-001：loader 读取正确 manifest
- 严重级别：P3
- 状态：通过
- 对规划/控制/车辆运行影响：不会把旧 `manifest.json` 当作主入口。
- 推荐修复：无。
- 验证方法：静态检查，并测试缺少 `project_manifest.json` 的负样例。

### F-RL-002：使用字符串搜索解析 JSON，鲁棒性不足
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：格式变化、嵌套字段、消息文本中出现关键字时可能误判。
- 推荐修复：统一使用 `YAML::Node` 或 JSON parser 结构化读取 schema、schema_version、package_id、coordinate_system、validation、hashes。
- 验证方法：构造字段重排、嵌套 status、message 中包含 failed、不同空白格式的测试包。

### F-RL-003：validation 逻辑可能误判
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：失败包可能被接受，合法包也可能因为 warning 文本包含 failed 而被拒绝。
- 推荐修复：精确读取 `validation_report["status"]` 和 `validation_report["summary"]["blocking_errors"]`，并同时读取 manifest validation。
- 验证方法：passed report 中 warning 文本包含 failed；failed report 使用不同空白格式。

### F-RL-004：未执行 checksum 校验
- 严重级别：P1
- 状态：失败
- 对规划/控制/车辆运行影响：runtime 无法保证包文件未损坏或未篡改。
- 推荐修复：添加 SHA-256 实现或依赖，比较 `checksums.sha256` 与 `manifest.hashes`。
- 验证方法：篡改 sample 包文件，校验开启时应拒绝加载。

### F-RL-005：字段和边界校验不完整
- 严重级别：P2
- 状态：部分通过
- 对规划/控制/车辆运行影响：坏 waypoint range 可能被截断或跳过；缺少必填字段时错误信息不精确。
- 推荐修复：验证 node/edge id、waypoint range 边界、必填 waypoint 字段、有限数值、manifest file target 是否存在。
- 验证方法：为每类缺失或非法字段创建负样例。

## 因环境无 ROS2 而跳过的命令
- SKIPPED_ROS2_UNAVAILABLE: `colcon test --packages-select low_speed_av_planning`
