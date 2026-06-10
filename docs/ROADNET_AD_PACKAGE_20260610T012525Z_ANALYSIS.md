# roadnet_ad_package_20260610T012525Z 路网包逐文件分析报告

## 1. 分析结论

`roadnet_ad_package_20260610T012525Z/` 是由路网编辑器导出的 Low Speed Roadnet AD Package v1.1 路网包。按照当前工程的 RoadnetLoader、规划模块和控制模块要求进行静态解析后，结论如下：

**总体可行：该目录可以作为当前规划模块的路网输入。**

可行原因：

- 入口文件是当前规范要求的 `project_manifest.json`，不是旧版 `manifest.json`。
- `schema` 为 `low_speed_roadnet_ad_package`。
- `schema_version` 为 `1.1.0`，在当前加载器支持范围内。
- `validation.status` 为 `warning`，不是 `failed`。
- `validation.blocking_errors` 为 `0`。
- `validation/validation_report.json` 中 `summary.blocking_errors` 也是 `0`。
- `checksums.sha256` 与所有被引用文件的实际 SHA-256 完全匹配。
- `roadnet/topology.json` 有 16 个节点、22 条边，所有边的 `from/to` 都能引用到有效节点。
- `trajectory/waypoints.yaml` 有 496 个 waypoint，字段完整，数值有限。
- `trajectory/waypoints.csv` 也有 496 行数据，与 YAML waypoint 数量一致。
- `trajectory/waypoint_index.json` 覆盖 22 条边，所有区间合法。
- 22 条 topology edge 都有 waypoint index 和 waypoint 数据。
- 没有发现旧主路径：`manifest.json`、`trajectory/waypoints.json`、根目录 `validation_report.json`。

需要注意：

- validation 状态是 `warning`，不是完全无告警；共有 32 条 warning，主要是高曲率和曲率连续性问题。
- `waypoint_index.json` 使用 v1.1.0 的 legacy inclusive `end_index`，没有 `end_index_exclusive`。当前 RoadnetLoader 支持这种 fallback，但会把它转换成内部半开区间。
- `vehicle_profile.wheel_base_m` 为 `null`。这不影响规划加载，但控制模块必须继续从 YAML 配置中提供 `vehicle.wheel_base_m`。
- `semantics/areas.json` 中包含 no-go 和 speed-zone，但按当前点级多边形判断，没有 waypoint 落在 no-go 或 speed-zone 内，因此当前规划不会因为这些语义区域阻断边或触发 speed-zone 限速。
- `parking_points.json` 与 `charging_points.json` 为空，不能用停车点或充电点作为目标；可以用 node id 或 task point。

## 2. 分析命令和结果

当前 Windows 环境中的 `python.exe` 是 Windows Store 占位符，直接运行 `python scripts\validate_sample_ad_package.py ...` 返回失败且无有效输出；`py` 命令不可用。因此使用 `uv` 临时 Python 环境执行分析。

离线校验命令：

```powershell
uv run --with pyyaml python scripts\validate_sample_ad_package.py roadnet_ad_package_20260610T012525Z
```

结果：

```text
AD Package OK: roadnet_ad_package_20260610T012525Z (16 nodes, 22 edges, 496 waypoints)
```

说明：

- 这个校验覆盖 canonical 文件存在性、manifest schema、validation 状态、topology 引用、waypoint 字段、waypoint index 范围、checksum 一致性。
- 校验通过代表该包满足当前工程离线验证脚本的基本输入要求。

## 3. 当前工程兼容性矩阵

| 检查项 | 当前包内容 | 当前代码要求 | 结论 |
|---|---|---|---|
| manifest 入口 | `project_manifest.json` 存在 | RoadnetLoader 从 `project_manifest.json` 读取 | 通过 |
| schema | `low_speed_roadnet_ad_package` | 必须匹配该 schema | 通过 |
| schema_version | `1.1.0` | 支持 `1.1.0` / `1.1.x` | 通过 |
| canonical topology | `roadnet/topology.json` | 必需 | 通过 |
| canonical waypoints | `trajectory/waypoints.yaml` | 必需 | 通过 |
| waypoint index | `trajectory/waypoint_index.json` | 必需 | 通过 |
| validation report | `validation/validation_report.json` | 必需 | 通过 |
| validation 状态 | `warning`，blocking_errors=0 | failed 或 blocking_errors>0 会拒绝 | 可加载，但有告警 |
| checksum | 全部匹配 | 开启 verify 时 mismatch 会拒绝 | 通过 |
| obsolete path | 未发现旧路径 | 不应使用旧路径 | 通过 |
| waypoint `kappa` | 存在 | 映射为内部 `kappa_1pm` | 通过 |
| waypoint `v_mps` | 存在 | 映射为内部 `target_speed_mps` | 通过 |
| `end_index_exclusive` | 未使用 | 优先支持 | 不使用 |
| legacy `end_index` | 22 条 edge 使用 | 当前代码支持 inclusive fallback | 通过，会产生兼容提示 |
| no-go 语义 | 2 个区域 | 当前代码按 no-go/keepout/allow_planning_through 阻断覆盖 waypoint 的 edge | 可解析，当前不阻断 |
| speed-zone 语义 | 1 个区域 | 当前代码可对区域内 waypoint 限速 | 可解析，当前不影响 waypoint |
| task points | 7 个 | 当前代码可加载 task point | 通过 |
| parking/charging | 均为空 | 可为空 | 通过，但不能作为目标 |

当前代码依据：

- `src/low_speed_av_planning/src/roadnet_loader.cpp:213`：读取 `project_manifest.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:223`：检查 `schema_version`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:253`：检查 manifest validation 和 blocking_errors。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:259`：读取 `validation_report`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:268`：读取 `topology`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:269`：读取 `waypoints_yaml`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:270`：读取 `waypoint_index`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:342`：将 waypoint `kappa` 映射为内部曲率。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:343`：将 waypoint `v_mps` 映射为内部目标速度。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:372`：优先支持 `end_index_exclusive`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:375`：legacy `end_index` fallback 为 inclusive。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:390`：读取 `semantics/areas.json`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:416`：读取 `route_points`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:419`：读取 `task_points`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:422`：读取 `parking_points`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:425`：读取 `charging_points`。
- `src/low_speed_av_planning/src/roadnet_loader.cpp:452`：执行 checksum 验证。

## 4. 总体统计

| 项目 | 数量/状态 |
|---|---:|
| 文件总数 | 23 |
| manifest files 条目 | 23 |
| manifest hashes 条目 | 21 |
| checksums.sha256 条目 | 22 |
| checksum 结果 | 全部匹配 |
| topology nodes | 16 |
| topology edges | 22 |
| topology edge 引用错误 | 0 |
| waypoint YAML 点数 | 496 |
| waypoint CSV 行数 | 496 |
| waypoint 缺失必需字段 | 0 |
| waypoint 非有限数值 | 0 |
| waypoint_index edges | 22 |
| waypoint_index 非法区间 | 0 |
| 使用 `end_index_exclusive` 的边 | 0 |
| 使用 legacy `end_index` 的边 | 22 |
| areas | 4 |
| route_points | 7 |
| task_points | 7 |
| parking_points | 0 |
| charging_points | 0 |
| validation warnings | 32 |
| validation blocking_errors | 0 |

## 5. 逐文件清单与作用说明

| 文件 | 大小 bytes | SHA-256 | 内容/作用 | 可用性 |
|---|---:|---|---|---|
| `checksums.sha256` | 2074 | `a9d198cd0c1b277f55cb5f21ad32ab0405ae6c2f5fe5dd84bd333016c64c3769` | 文件校验清单，包含包内主要文件 hash | 可用，全部匹配 |
| `project_manifest.json` | 4541 | `ba12373dcc4d49c929da6de0acd109dd6830dcab2f546f472bb7dab6651097a5` | AD Package 入口 manifest，声明 schema、版本、文件路径、hash、坐标系、单位和 validation 摘要 | 可用 |
| `examples/mission.example.json` | 321 | `f759c0516fcfacdbbb23c19f04dabe50722aef3ca5ac481f193149f1e721b603` | 示例任务，请求从 `N0001` 到 `N0016` | 可用作人工规划测试样例 |
| `map/map_metadata.yaml` | 294 | `a8f83f4423e763e0534d8d4841a19d39ca0276bae19b11aae62bd662b9568ec1` | 地图元数据，frame、origin、resolution、width/height | 可用，规划主要使用 frame/单位参考 |
| `roadnet/roadnet.json` | 55646 | `a64222c9e3b88009caf7703c4a2eac95bfebcb817ffbade75bdfb30bac0dc07e` | 几何路网原始结构，含 nodes、paths、areas、route_points | 可用作编辑器语义/几何参考；当前规划主要用 topology/waypoints |
| `roadnet/route_graph.yaml` | 2909 | `ade22a75d178e206e3b474bc548a5eedcd02ec6d1b742cc8b1edff34c58c7f1e` | 简化路线图 YAML，含 16 nodes、22 edges | 可用作人工阅读和检查；当前加载器主要读取 topology.json |
| `roadnet/topology.json` | 116498 | `baeeffba27635a4449f8483233b9ced315d6b6279a36044d1157a9072994a49f` | 规划主拓扑，含节点、边、cost、方向、约束、reference_points | 可用，是全局规划核心输入 |
| `schemas/project_manifest.schema.json` | 4354 | `1e1adcb8126d03934142348dd2e82720165c6062a3dc39b4098fa0c812d0d8ba` | manifest JSON Schema | 可用作格式校验参考 |
| `schemas/roadnet.schema.json` | 4863 | `eb8a05cdc052954f9057025ff7a7db36915cff100d1ab04b25310ca85425e537` | roadnet JSON Schema | 可用作格式校验参考 |
| `schemas/semantics.schema.json` | 6500 | `2d883c6528e68f5f5b54e19fb542fdbb6a44090e8e08d6933cfa7809947cddbb` | semantics 文件 Schema | 可用作格式校验参考 |
| `schemas/topology.schema.json` | 4084 | `36c64605cf92dff21fdaad41abc98917d07674c1d34c2a3036974187354d6731` | topology JSON Schema | 可用作格式校验参考 |
| `schemas/validation_report.schema.json` | 1961 | `f644ef0682a0c4453d91f053507f5925530dd89736bd0cc00c12c0fa8119a446` | validation report JSON Schema | 可用作格式校验参考 |
| `schemas/waypoint_index.schema.json` | 1387 | `fb6fcda6b281b8b7548f5bb31151daa1bd9584a861a5a6755cf98f5986183a07` | waypoint index JSON Schema | 可用作格式校验参考 |
| `schemas/waypoints.schema.json` | 2232 | `0d60bd3a8cfdd24eea7f325d03df22c3f196724400e399e95a4f08cde78b4c34` | waypoints YAML Schema | 可用作格式校验参考 |
| `semantics/areas.json` | 2453 | `db97ed9a0b0d1f153042a2b2c9679449085f32a7e76229df43fce961f25b832a` | 4 个语义区域：drivable/no_go/speed_zone | 可用，当前不覆盖 waypoint |
| `semantics/charging_points.json` | 127 | `6bfb5265cbf84f0fa3c9d12583f83b7b27103bb968ffd82aba392f5bcef4a9fe` | 充电点集合，当前为空 | 可用但无充电目标 |
| `semantics/parking_points.json` | 125 | `c550c43b087d93bc3a4314fad033cc5fcc9261fdc1c3ff1d32b4ab582f48b5cd` | 停车点集合，当前为空 | 可用但无停车目标 |
| `semantics/route_points.json` | 4896 | `0de6ad1f3eb08c96017ec022489ccccbe80c5fb133bcc6cb4fe073f8f802b6a4` | 7 个 route point，包含任务点语义和 pose | 可用作语义参考 |
| `semantics/task_points.json` | 5265 | `c744a14cf0903abafe51a547f7565739a17c805f67a64d4f334f45d8a525cccc` | 7 个 task point，已绑定 linked_edge_id 和 linked_path_id | 可用，适合作为任务点目标 |
| `trajectory/waypoint_index.json` | 5607 | `82dd48e3e09ad50b921e6fc0ccce01fe6e4cf5e6b138302a6ab4654f99e280d0` | edge 到 waypoint 区间的索引，共 22 条 | 可用，使用 legacy inclusive `end_index` |
| `trajectory/waypoints.csv` | 52661 | `1d479ecf494db1b21f61b2dd687b5ed673a3d4862c41fec6cd825e8718c3aad8` | waypoint 表格副本，496 行 | 可用作人工检查；当前加载器使用 YAML |
| `trajectory/waypoints.yaml` | 140000 | `fcf7ee920fd32e1b7ff7cc0768d7e6b48ede99375607732fce27d72bca784e93` | 规划轨迹点主输入，496 个点 | 可用，是 motion planner 拼接轨迹的核心输入 |
| `validation/validation_report.json` | 9562 | `96088e8f6ba6d970b981478a1bbaf8e209dd2bfd511e88cdea57c58a2ecc07d1` | 校验报告，status warning、blocking_errors 0 | 可加载，但需关注 warning |

## 6. 文件内容与数据结构详解

### 6.1 `project_manifest.json`

关键内容：

```text
schema: low_speed_roadnet_ad_package
schema_version: 1.1.0
package_id: pkg_fc93b922-8948-4e0f-ba4b-e6cc66b08c4a_20260610T012525Z
exported_at: 2026-06-10T01:25:25Z
export_source: backend
project.name: pytest 园区项目
coordinate_system.global_frame: map
coordinate_system.control_reference_frame: rear_axle
units.distance: m
units.angle: rad
units.speed: mps
units.curvature: 1/m
validation.status: warning
validation.blocking_errors: 0
```

`files` 中列出了当前工程需要的关键路径，包括：

- `topology -> roadnet/topology.json`
- `waypoints_yaml -> trajectory/waypoints.yaml`
- `waypoint_index -> trajectory/waypoint_index.json`
- `validation_report -> validation/validation_report.json`
- `areas -> semantics/areas.json`
- `task_points -> semantics/task_points.json`
- `parking_points -> semantics/parking_points.json`
- `charging_points -> semantics/charging_points.json`
- `route_points -> semantics/route_points.json`

兼容性判断：

- 当前 RoadnetLoader 会优先通过 manifest files 解析这些文件，完全匹配当前工程约定。
- `vehicle_profile.wheel_base_m` 为 `null`，不应由路网包决定车辆轴距；控制模块仍需使用自身 YAML 参数。

### 6.2 `checksums.sha256`

内容结构：

```text
<sha256>  <relative/path>
```

统计：

- 共有 22 条 checksum 记录。
- 所有记录引用的文件都存在。
- 所有实际 SHA-256 都与记录匹配。

兼容性判断：

- 当前 `roadnet.verify_checksums=true` 时，该包可以通过 checksum 校验。
- 如果后续从 Windows 拷贝到 Ubuntu，要保持文本文件换行不被自动转换，否则 checksum 可能失配。

### 6.3 `map/map_metadata.yaml`

关键内容：

```text
schema: low_speed_map_metadata
schema_version: 1.1.0
frame_id: map
resolution_m_per_px: 0.02
origin.x: -28.771
origin.y: -4.388
origin.yaw: 0.0
width_px: 2540
height_px: 910
image: null
```

兼容性判断：

- 当前规划和控制链路主要使用 metric coordinate，不依赖地图图片，因此 `image: null` 不影响规划输入。
- 定位模块发布的 `/localization/pose.header.frame_id` 建议与这里的 `map` 一致。

### 6.4 `roadnet/topology.json`

关键结构：

```text
schema: low_speed_topology
schema_version: 1.1.0
nodes: 16
edges: 22
```

节点结构示例：

```json
{
  "id": "N0001",
  "pose": {"x": 0.554, "y": 1.473, "yaw": -0.9109},
  "type": "normal",
  "source_node_id": "C-001-START"
}
```

边结构示例：

```json
{
  "id": "E_C-001_F",
  "from": "N0001",
  "to": "N0002",
  "direction": "forward",
  "cost": 1.982,
  "length_m": 1.982,
  "speed_limit_mps": 1.0,
  "constraints": {
    "allow_reverse": false,
    "allow_stop": true,
    "max_curvature_abs": 0.35,
    "max_speed_mps": 1.0,
    "min_speed_mps": 0.3
  },
  "reference_points": [...]
}
```

兼容性判断：

- 当前 Dijkstra/A* 使用 nodes、edges、edge cost、direction、enabled/blocked 信息。
- 所有 edge 的 `from` 和 `to` 均引用有效节点。
- 22 条 edge 均存在对应 waypoint index。
- 支持正向和倒车边，当前包中 reverse 边包括 `E_L-008_R`、`E_L-010_R`、`E_C-007_R`、`E_L-012_R`。

### 6.5 `roadnet/route_graph.yaml`

关键结构：

```text
schema: low_speed_route_graph
schema_version: 1.1.0
coordinate_frame: map
nodes: 16
edges: 22
```

用途：

- 这是更轻量的路线图 YAML，便于人工查看节点/边关系。
- 当前 RoadnetLoader 主路径读取 `topology.json`，不依赖该文件完成规划。

兼容性判断：

- 可作为调试、人工检查、外部工具可视化输入。
- 不影响当前规划能否运行。

### 6.6 `roadnet/roadnet.json`

关键结构：

```text
schema: low_speed_roadnet
schema_version: 1.1.0
coordinate_frame: map
nodes: 16
paths: 19
areas: 4
route_points: 7
```

用途：

- 保存编辑器侧更原始的几何和语义对象。
- 当前规划主流程主要依赖 `topology.json`、`waypoints.yaml`、`waypoint_index.json`。

兼容性判断：

- 文件存在且 hash 正确。
- 可用于后续更完整的地图可视化、语义回溯和编辑器调试。

### 6.7 `trajectory/waypoints.yaml`

关键结构：

```text
metadata: ...
waypoints:
  - global_index
    waypoint_id
    edge_id
    path_id
    node_from
    node_to
    s_m
    x
    y
    yaw
    kappa
    v_mps
    speed_limit_mps
    behavior
    direction
    flags
```

统计：

- waypoint 数量：496。
- 必需字段缺失：0。
- 非有限数值：0。
- 覆盖 edge 数量：22。

首个 waypoint：

```text
global_index: 0
waypoint_id: WP_E_C-001_F_000000
edge_id: E_C-001_F
path_id: C-001
x: 0.554
y: 1.473
yaw: -0.9178
kappa: -0.047082
v_mps: 1.0
direction: forward
flags: edge_start
```

最后一个 waypoint：

```text
global_index: 495
waypoint_id: WP_E_L-012_R_000018
edge_id: E_L-012_R
path_id: L-012
x: -0.716
y: 4.665
yaw: 1.9495
kappa: 0.0
v_mps: 1.0
direction: reverse
flags: edge_end
```

兼容性判断：

- 当前 RoadnetLoader 会读取 `x/y/yaw/s_m/kappa/v_mps/edge_id/path_id/direction/flags`。
- `kappa` 会映射为内部 `kappa_1pm`。
- `v_mps` 会映射为内部 `target_speed_mps`。
- 格式满足当前 motion planner 和 speed planner 拼接轨迹的输入要求。

### 6.8 `trajectory/waypoints.csv`

CSV header：

```text
global_index, waypoint_id, edge_id, path_id, node_from, node_to,
s_m, x, y, yaw, kappa, v_mps, speed_limit_mps, behavior, direction
```

统计：

- 数据行数：496。
- 与 YAML waypoint 数量一致。

兼容性判断：

- 当前规划加载主路径使用 YAML，不使用 CSV。
- CSV 可作为人工检查、外部工具导入或调试比较文件。

### 6.9 `trajectory/waypoint_index.json`

关键结构：

```text
schema: low_speed_waypoint_index
schema_version: 1.1.0
edges: 22
paths: ...
```

edge index 示例：

```json
{
  "E_C-001_F": {
    "direction": "forward",
    "path_id": "C-001",
    "start_index": 0,
    "end_index": 10
  }
}
```

统计：

- edge index 数量：22。
- 非法 index 区间：0。
- 使用 `end_index_exclusive`：0。
- 使用 legacy inclusive `end_index`：22。

兼容性判断：

- 当前 RoadnetLoader 支持 `end_index` fallback，会转换为内部 `end_index_exclusive = end_index + 1`。
- 因此该文件可用。
- 后续编辑器如果升级，建议导出 `end_index_exclusive`，减少 legacy 兼容提示。

## 7. topology edge 与 waypoint index 对应表

| Edge | From | To | Direction | Length m | Cost | Speed m/s | Waypoint start | Waypoint end inclusive |
|---|---|---|---|---:|---:|---:|---:|---:|
| `E_C-001_F` | N0001 | N0002 | forward | 1.982 | 1.982 | 1.0 | 0 | 10 |
| `E_L-001_F` | N0002 | N0003 | forward | 3.48 | 3.48 | 1.0 | 11 | 29 |
| `E_C-002_F` | N0003 | N0004 | forward | 2.787 | 2.787 | 1.0 | 30 | 44 |
| `E_L-002_F` | N0004 | N0005 | forward | 4.021 | 4.021 | 1.0 | 45 | 66 |
| `E_C-003_F` | N0005 | N0016 | forward | 3.068 | 3.068 | 1.0 | 67 | 83 |
| `E_C-005_F` | N0016 | N0006 | forward | 3.908 | 3.908 | 1.0 | 84 | 104 |
| `E_L-003_F` | N0006 | N0012 | forward | 2.431 | 2.431 | 1.0 | 105 | 118 |
| `E_L-006_F` | N0012 | N0007 | forward | 3.801 | 3.801 | 1.0 | 119 | 139 |
| `E_C-004_F` | N0007 | N0008 | forward | 4.372 | 4.372 | 1.0 | 140 | 162 |
| `E_C-006_F` | N0009 | N0010 | forward | 4.416 | 4.416 | 1.0 | 163 | 186 |
| `E_L-004_F` | N0010 | N0014 | forward | 4.784 | 4.784 | 1.0 | 187 | 211 |
| `E_L-009_F` | N0014 | N0011 | forward | 4.929 | 4.929 | 1.0 | 212 | 237 |
| `E_L-005_F` | N0011 | N0013 | forward | 7.664 | 7.664 | 1.0 | 238 | 277 |
| `E_L-007_F` | N0013 | N0001 | forward | 7.698 | 7.698 | 1.0 | 278 | 317 |
| `E_L-008_F` | N0012 | N0015 | forward | 2.094 | 2.094 | 1.0 | 318 | 329 |
| `E_L-008_R` | N0015 | N0012 | reverse | 2.094 | 2.513 | 1.0 | 330 | 341 |
| `E_L-010_F` | N0015 | N0013 | forward | 2.701 | 2.701 | 1.0 | 342 | 356 |
| `E_L-010_R` | N0013 | N0015 | reverse | 2.701 | 3.241 | 1.0 | 357 | 371 |
| `E_C-007_F` | N0014 | N0015 | forward | 9.373 | 9.373 | 1.0 | 372 | 419 |
| `E_C-007_R` | N0015 | N0014 | reverse | 9.373 | 11.248 | 1.0 | 420 | 467 |
| `E_L-011_F` | N0009 | N0008 | forward | 1.49 | 1.49 | 1.0 | 468 | 476 |
| `E_L-012_R` | N0001 | N0016 | reverse | 3.436 | 4.123 | 1.0 | 477 | 495 |

## 8. 语义数据分析

### 8.1 `semantics/areas.json`

区域统计：

| ID | Type | Polygon 点数 | speed_limit_mps | allow_planning_through | 覆盖 waypoint 数量 | 影响 edge |
|---|---|---:|---:|---|---:|---|
| `R-001` | drivable_area | 8 | 2.0 | true | 496 | 全部 22 条 edge |
| `R-002` | no_go_area | 7 | 2.0 | true | 0 | 无 |
| `R-003` | no_go_area | 4 | 2.0 | true | 0 | 无 |
| `R-004` | speed_zone | 4 | 2.0 | true | 0 | 无 |

兼容性判断：

- 当前 RoadnetLoader 会加载这些区域。
- 当前代码把 `type == no_go_area`、`type == keepout` 或 `allow_planning_through == false` 的区域作为阻断区域。
- 由于两个 no-go 区域没有覆盖任何 waypoint，因此不会阻断任何 edge。
- speed-zone 区域也没有覆盖 waypoint，因此当前不会降低轨迹速度。

### 8.2 `semantics/route_points.json`

统计：

- route_points 数量：7。
- 每个 route point 有 `id`、`type`、`action`、`pose`、`properties`。
- `route_points.json` 中的 `linked_edge_id` 多为 null，但 `properties.path_id` 和 `s_on_path` 存在。

兼容性判断：

- 可作为原始 route point 语义参考。
- 当前规划更适合使用已经绑定 edge 的 `task_points.json`。

### 8.3 `semantics/task_points.json`

统计：

| Task point | linked_edge_id | linked_path_id | Pose |
|---|---|---|---|
| `RP-001` | `E_L-011_F` | `L-011` | x=-13.283, y=8.987, yaw=-0.5485 |
| `RP-002` | `E_L-003_F` | `L-003` | x=-6.348, y=5.251, yaw=-3.1291 |
| `RP-003` | `E_L-007_F` | `L-007` | x=-4.522, y=0.792, yaw=0.1333 |
| `RP-004` | `E_L-008_F` | `L-008` | x=-7.005, y=4.053, yaw=-1.5902 |
| `RP-005` | `E_L-009_F` | `L-009` | x=-15.585, y=2.306, yaw=-1.2631 |
| `RP-006` | `E_C-002_F` | `C-002` | x=6.164, y=1.464, yaw=1.5397 |
| `RP-007` | `E_C-001_F` | `C-001` | x=0.768, y=1.189, yaw=-0.9392 |

兼容性判断：

- 当前 loader 可读取 task_points。
- 这些 task point 已经绑定到 edge/path，比 route_points 更适合作为任务目标解析基础。

### 8.4 `semantics/parking_points.json`

内容：

```text
parking_points: []
```

兼容性判断：

- 空数组合法。
- 当前包不能测试 `goal_parking_point_id` 规划目标。

### 8.5 `semantics/charging_points.json`

内容：

```text
charging_points: []
```

兼容性判断：

- 空数组合法。
- 当前包不能测试充电点目标。

## 9. validation report 分析

`validation/validation_report.json`：

```text
schema: low_speed_validation_report
schema_version: 1.1.0
status: warning
score: 0.0
summary.nodes: 16
summary.edges: 22
summary.paths: 19
summary.waypoints: 496
summary.areas: 4
summary.parking_points: 0
summary.warnings: 32
summary.blocking_errors: 0
```

warning code：

- `HIGH_CURVATURE`
- `CURVATURE_CONTINUITY`
- `TOPOLOGY_EDGE_HIGH_CURVATURE`
- `WAYPOINT_CURVATURE_EXCEEDS_CONSTRAINT`

关键影响：

- 这些 warning 不会导致当前 RoadnetLoader 拒绝加载。
- 但它们会影响实车安全余量，尤其是转角、转角速度、横向加速度和 LQR/Stanley/Pure Pursuit 跟踪稳定性。
- 建议实车前用低速、低加速度、轮离地或封闭场地验证，并重点观察 `E_C-004_F`、`E_C-006_F` 以及 validation 中提到的高曲率 path。

## 10. schemas 文件分析

| Schema 文件 | Title | Required/用途 |
|---|---|---|
| `schemas/project_manifest.schema.json` | Low Speed Roadnet AD Package Manifest | 要求 schema、schema_version、package_id、project、map、vehicle_profile、coordinate_system、units、files、hashes、validation |
| `schemas/roadnet.schema.json` | Low Speed Roadnet Geometry | 要求 roadnet geometry 的 nodes、paths、areas、route_points、metadata |
| `schemas/semantics.schema.json` | Low Speed Semantics Files | 语义文件通用定义 |
| `schemas/topology.schema.json` | Low Speed Topology | 要求 map_id、graph_id、coordinate_frame、nodes、edges |
| `schemas/validation_report.schema.json` | Low Speed Validation Report | 要求 status、score、summary、items |
| `schemas/waypoint_index.schema.json` | Low Speed Waypoint Index | 要求 edges、paths |
| `schemas/waypoints.schema.json` | Low Speed Waypoints | 要求 metadata、waypoints |

兼容性判断：

- schemas 文件不是当前 RoadnetLoader 的运行时必需输入，但它们有助于后续编辑器、CI 或离线工具做严格格式校验。
- 当前包自带 schemas，有利于长期维护和跨平台一致性校验。

## 11. 示例任务可行性

`examples/mission.example.json`：

```text
start_node_id: N0001
goal_node_id: N0016
required_consumers:
  - global_planner
  - motion_planner
  - controller
  - task_manager
  - safety_validation
```

用拓扑做简单 Dijkstra 静态检查：

| Start | Goal | 可达性 | 最短 edge 序列 | Cost |
|---|---|---|---|---:|
| `N0001` | `N0003` | 可达 | `E_C-001_F -> E_L-001_F` | 5.462 |
| `N0001` | `N0016` | 可达 | `E_L-012_R` | 4.123 |
| `N0009` | `N0008` | 可达 | `E_L-011_F` | 1.49 |
| `N0015` | `N0012` | 可达 | `E_L-008_R` | 2.513 |

因此示例 mission `N0001 -> N0016` 可用于测试规划服务。

示例 ROS2 调用：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0016', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

## 12. 当前包作为规划输入时的推荐配置

规划参数建议：

```yaml
roadnet:
  package_path: "/absolute/path/to/roadnet_ad_package_20260610T012525Z"
  reject_failed_validation: true
  verify_checksums: true

global_planner:
  algorithm: "astar"
  allow_reverse: true

motion_planner:
  algorithm: "reference_line"
  horizon_distance_m: 15.0

speed_planner:
  algorithm: "curvature"
  default_speed_mps: 0.5
  max_speed_mps: 1.0
  max_lateral_accel_mps2: 0.5
```

控制参数建议：

```yaml
topics:
  localization_pose_topic: "/localization/pose"
  trajectory_topic: "/planning/trajectory"
  scu_command_topic: "/yunle_chassis/control/scu_control_command"

vehicle:
  model: "front_ackermann"   # 或 dual_ackermann
  wheel_base_m: 1.2          # 必须来自车辆配置，不应依赖本包 manifest

controller:
  algorithm: "pure_pursuit"  # 初次联调建议先用 pure_pursuit，再测试 lqr
```

## 13. 不可直接作为定位输入

这个 AD Package 是路网和轨迹参考数据，不是定位数据源。它不能替代定位模块。

控制模块仍需要外部定位发布：

```text
/localization/pose
geometry_msgs/msg/PoseStamped
```

定位数据应满足：

- frame 建议为 `map`，与 `map_metadata.yaml` 和 manifest 的 `global_frame` 一致。
- 坐标单位为米。
- yaw 使用弧度语义，ROS message 中通过 quaternion 表达。
- 车辆参考点最好与 `control_reference_frame: rear_axle` 一致。

## 14. 风险与建议

### 14.1 P1：高曲率 warning 可能影响车辆可跟踪性

证据：

- validation report 中存在 `HIGH_CURVATURE`、`TOPOLOGY_EDGE_HIGH_CURVATURE`、`WAYPOINT_CURVATURE_EXCEEDS_CONSTRAINT`。
- 部分 path 的最大曲率远超推荐值。

影响：

- 低速仿真可能可跑，但实车可能出现转角饱和、跟踪误差大、速度规划强制降速或底盘执行困难。

建议：

- 在路网编辑器中平滑高曲率 path。
- 初次实车设置 `speed_planner.max_speed_mps <= 0.5`。
- 检查控制输出前后轮转角是否触发 SCU 角度限幅。

### 14.2 P2：`end_index_exclusive` 缺失

证据：

- 22 条 edge 均使用 legacy `end_index`。

影响：

- 当前代码兼容，不影响运行。
- 但会依赖 fallback 逻辑，长期建议编辑器导出 `end_index_exclusive`。

建议：

- 后续导出器升级为同时输出 `end_index_exclusive` 和 `count`。

### 14.3 P2：speed-zone/no-go 区域当前不影响 waypoint

证据：

- no-go 区域 R-002/R-003 覆盖 waypoint 数量为 0。
- speed-zone 区域 R-004 覆盖 waypoint 数量为 0。

影响：

- 这不是格式错误，但当前包无法验证“no-go 阻断”和“speed-zone 限速”的实际规划效果。

建议：

- 若要测试语义约束，应导出一个 no-go 覆盖某条边或 speed-zone 覆盖部分 waypoint 的测试包。

### 14.4 P3：parking/charging point 为空

影响：

- 无法测试 `goal_parking_point_id` 或充电任务。

建议：

- 如果业务需要停车/充电任务，后续在编辑器补充相应语义点。

## 15. 最终判断

该路网包满足当前工程的 AD Package v1.1 输入合同，可以作为当前规划模块的路网输入，并可进一步驱动控制模块输出底盘命令，前提是：

1. `roadnet.package_path` 指向该目录。
2. `roadnet.verify_checksums` 保持 true 时文件内容不能被换行转换或手工修改。
3. `roadnet.reject_failed_validation` 保持 true 时，由于 blocking_errors 为 0，当前包可加载。
4. 外部定位模块持续发布 `/localization/pose`。
5. 控制模块配置了正确的车辆参数，尤其是 `vehicle.wheel_base_m`。
6. 初次运行采用低速、仿真或台架验证，重点观察高曲率路径上的控制输出。

推荐第一条规划测试路线：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0003', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

推荐示例 mission 路线：

```bash
ros2 service call /low_speed_av_planning/plan_route \
  low_speed_av_interfaces/srv/PlanRoute \
  "{start_node_id: 'N0001', goal_node_id: 'N0016', start_task_point_id: '', goal_task_point_id: '', goal_parking_point_id: ''}"
```

最终输出链路：

```text
roadnet_ad_package_20260610T012525Z
  -> RoadnetLoader
  -> Dijkstra/A*
  -> reference_line trajectory
  -> curvature/constant speed planner
  -> /planning/trajectory
  -> control node + /localization/pose
  -> controller + Ackermann model
  -> ScuCommandMapper
  -> /yunle_chassis/control/scu_control_command
```
