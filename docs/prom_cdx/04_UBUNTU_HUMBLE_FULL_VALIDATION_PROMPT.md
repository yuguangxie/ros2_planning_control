# Ubuntu 22.04 + ROS2 Humble全项目验证Prompt

以下Prompt用于在Ubuntu 22.04、ROS2 Humble环境clone当前仓库后，对Phase 13—16及整个ROS2工作区执行真实构建、测试和证据审计。

````markdown
你现在位于Ubuntu 22.04系统中的ROS2低速自动驾驶项目根目录。系统已安装ROS2 Humble。本任务只做当前提交的完整验证、问题定位和结果报告，不实施新的Planning、Control或Chassis生产功能，不为了让测试通过而修改正式Roadnet数据。

# 一、验证目标

对当前clone的整个项目执行可复现、绑定commit SHA的真实验证，重点确认：

1. 所有ROS2 package能够在Humble下完成依赖解析和全量编译；
2. Planning、Control和当前Chassis core的production-linked C++ gtest实际编译并执行；
3. Planning与Control ROS2 launch/integration test实际执行，且不会连接真实底盘；
4. sample、正式Roadnet `_1`、正式Roadnet `_2`、template/config/repository checks实际通过；
5. Phase 13安全语义、Phase 15 Planning加固和Phase 16 Control加固没有被编译或运行期问题破坏；
6. 当前CI workflow与本地Ubuntu结果一致；
7. 输出一份包含精确命令、测试数量、PASS/FAIL/SKIPPED、日志位置和finding状态的报告。

本任务不是功能开发任务。除新增验证报告和原始结果索引外，不修改生产代码、配置、测试期望、正式fixture或历史报告。如果发现失败，先完整定位并报告，不要顺手修复；只有用户随后明确授权才能进入修复任务。

# 二、必须先阅读

完整阅读：

- 根目录`AGENTS.md`
- `docs/change/00_INDEX.md`
- `docs/change/01_PHASE_13_CONTROL_SAFETY.md`
- `docs/change/02_PHASE_14_TEST_INFRASTRUCTURE.md`
- `docs/change/03_PHASE_15_PLANNING_INTEGRITY.md`
- `docs/change/04_PHASE_16_CONTROL_ENGINEERING.md`
- `docs/change/05_FOUR_PHASE_OVERVIEW.md`
- `reports/phase_13_report.md`
- `reports/phase_14_report.md`
- `reports/phase_15_report.md`
- `reports/phase_16_report.md`
- `reports/final_generation_report.md`
- `docs/PHASE14_TEST_MATRIX.md`
- `docs/PHASE15_TEST_MATRIX.md`
- `docs/PHASE16_TEST_MATRIX.md`
- `docs/HARDWARE_WATCHDOG_500MS_VALIDATION.md`
- `docs/audit_cdx/03_FINDINGS_AND_RISK_REGISTER.md`
- `docs/audit_cdx/04_TESTING_DOCUMENTATION_AND_ENGINEERING.md`
- `docs/03_ros2_interfaces.md`
- `docs/04_planning_module_design.md`
- `docs/05_control_module_design.md`
- `docs/07_config_launch_runtime.md`
- `docs/YUNLE_SCU_COMMAND_OUTPUT.md`
- `src/low_speed_av_planning/CMakeLists.txt`
- `src/low_speed_av_planning/package.xml`
- `src/low_speed_av_planning/test/**`
- `src/low_speed_av_control/CMakeLists.txt`
- `src/low_speed_av_control/package.xml`
- `src/low_speed_av_control/test/**`
- `src/low_speed_av_bringup/CMakeLists.txt`
- `src/low_speed_av_bringup/package.xml`
- `src/low_speed_av_bringup/test/**`
- `src/yunle_chassis/chassis_driver/CMakeLists.txt`
- `src/yunle_chassis/chassis_driver/package.xml`
- `src/yunle_chassis/chassis_driver/test/**`
- `scripts/run_offline_checks.py`
- `scripts/check_control_config_contract.py`
- `scripts/check_template_consistency.py`
- `scripts/offline_repository_hygiene.py`
- `.github/workflows/ros2_humble_ci.yml`

文件不存在时在报告中记录`MISSING`，不得假定存在。

# 三、已知状态和证据边界

开始前按源码重新核验，不要只相信报告：

- Phase 13实现了Trajectory安全元数据消费、Control安全状态机、VehicleState门控、显式estop clear和默认双输出；
- Phase 14建立了Planning、Control和当前Chassis core的production-linked gtest、ROS2 integration source、统一offline runner和GitHub Actions；
- Phase 15实现了Planning path containment、Loader结构校验、Dijkstra/A*合同、semantic/trajectory helper和Planning progress；
- Phase 16实现了Control参数fail-fast、真实dt smoother、controller fail-closed、Control progress、cadence诊断和Control-only launch source；
- Windows环境已执行offline runner 18/18 PASS，但Phase 15/16新增C++和ROS2测试此前是`GENERATED_NOT_EXECUTED`或`SKIPPED_ROS2_UNAVAILABLE`；
- 历史GitHub run `29152378189`总体`EXECUTED_FAIL`，不能作为当前提交full PASS；
- `CDX-P0-002`仍是`OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`；
- “500 ms无CAN 0x121由底盘停车”是项目方声明，当前为`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`；
- 纯Ubuntu/ROS2测试不能把真实硬件watchdog写成HIL PASS。

# 四、硬约束

1. 不修改`src/low_speed_av_planning/**`、`src/low_speed_av_control/**`、`src/yunle_chassis/**`等生产代码。
2. 不修改CMake、package.xml、测试源码或测试期望来掩盖失败。
3. 不修改sample、正式`_1`、正式`_2`、template sample内容。
4. 不启动`keyboard_scu_control_node`。
5. 不启动会连接真实`192.168.x.x`网关的Chassis Driver生产模式。
6. 不发送真实UDP、CAN或运动命令。
7. Chassis测试只允许production core gtest、fake transport、test mode或127.0.0.1动态端口。
8. 所有launch/integration必须有timeout，禁止无限等待。
9. Python replica或源码token检查不能作为production C++ PASS证据。
10. workflow文件存在不等于CI PASS。
11. 不执行真实车辆或台架故障注入；HIL统一标记`HIL_NOT_EXECUTED`。
12. 不reset、clean或删除用户已有内容。全新clone若需要重建，优先使用独立build/install/log目录。
13. 允许安装测试所需的Ubuntu/ROS2标准依赖，但必须记录实际安装命令和失败项。
14. 只允许新增一份本次验证报告和必要的非提交原始日志目录；不要更新Phase 13—16历史结论来伪造成功。

# 五、开始前基线

先执行并记录完整输出：

```bash
pwd
git status --short
git rev-parse HEAD
git branch --show-current
git remote -v
git log -1 --oneline --decorate
uname -a
cat /etc/os-release
echo "ROS_DISTRO=${ROS_DISTRO:-unset}"
which ros2 || true
which colcon || true
which cmake || true
which c++ || true
which gcc || true
which g++ || true
which clang || true
which clang++ || true
which python3 || true
python3 --version
cmake --version
c++ --version
ros2 --help >/dev/null && echo ROS2_COMMAND_PASS
colcon version-check || true
```

如果`ROS_DISTRO`不是`humble`，先执行：

```bash
source /opt/ros/humble/setup.bash
```

然后再次记录`ROS_DISTRO`、`ros2 pkg list`和`colcon list`。如果`/opt/ros/humble/setup.bash`不存在，则停止构建部分并将其标记为`BLOCKED_ROS2_HUMBLE_MISSING`，仍继续执行不依赖ROS2的检查。

记录仓库污染基线：

```bash
git status --short
git ls-files build install log '.pytest_cache' '**/__pycache__'
find . -maxdepth 3 -type d \( -name build -o -name install -o -name log -o -name __pycache__ -o -name .pytest_cache \) -print
```

# 六、依赖解析

在仓库根目录执行：

```bash
source /opt/ros/humble/setup.bash
sudo apt-get update
rosdep update
rosdep check --from-paths src --ignore-src -r || true
sudo rosdep install --from-paths src --ignore-src -r -y
```

额外确认测试工具存在：

```bash
sudo apt-get install -y \
  python3-pip python3-pytest python3-yaml \
  clang-format \
  ros-humble-ament-cmake-gtest \
  ros-humble-launch-testing \
  ros-humble-launch-testing-ament-cmake
```

记录rosdep安装结果。依赖安装失败时，不要跳过错误文本；报告具体package、apt源和返回码。

# 七、源码与注册静态核验

执行并记录：

```bash
colcon list
grep -R "ament_add_gtest\|add_launch_test" -n \
  src/low_speed_av_planning \
  src/low_speed_av_control \
  src/low_speed_av_bringup \
  src/yunle_chassis/chassis_driver
grep -R "target_link_libraries(test_" -n \
  src/low_speed_av_planning \
  src/low_speed_av_control \
  src/yunle_chassis/chassis_driver
```

建立实际target矩阵，至少包含：

| Package | Production target | Test target | Registered | Direct production link | Expected cases |
|---|---|---|---|---|---:|
| low_speed_av_planning | low_speed_av_planning | test_roadnet_loader | 核验 | 核验 | 19 |
| low_speed_av_planning | low_speed_av_planning | test_planning_algorithms | 核验 | 核验 | 13 |
| low_speed_av_planning | low_speed_av_planning | test_planning_helpers | 核验 | 核验 | 8 |
| low_speed_av_control | low_speed_av_control | test_controllers | 核验 | 核验 | 6 |
| low_speed_av_control | low_speed_av_control | test_vehicle_command_pipeline | 核验 | 核验 | 11 |
| low_speed_av_control | low_speed_av_control | test_safety_state_machine | 核验 | 核验 | 6 |
| low_speed_av_control | low_speed_av_control | test_control_runtime_helpers | 核验 | 核验 | 5 |
| chassis_driver | chassis_driver_core | test_chassis_core | 核验 | 核验 | 按源码统计 |

Expected与实际不一致时记录差异，不要直接改源码。

# 八、离线与仓库治理检查

在任何colcon build之前执行：

```bash
python3 scripts/run_offline_checks.py
python3 scripts/check_control_config_contract.py
python3 scripts/check_template_consistency.py
python3 scripts/offline_repository_hygiene.py
python3 -m compileall -q scripts src/low_speed_av_bringup/test
git diff --check
```

必须在报告中逐项列出统一runner内每个检查的PASS/FAIL/SKIPPED，并单独记录：

- sample validator节点/边/waypoint数量；
- 正式`_1`节点/边/waypoint数量；
- 正式`_2`节点/边/waypoint数量；
- Control YAML leaf/declare/get数量；
- template/sample hash结果；
- UTF-8、JSON和Markdown link结果。

# 九、格式检查

按workflow中的实际文件列表执行同等级检查：

```bash
clang-format --version
clang-format --dry-run --Werror \
  src/low_speed_av_planning/test/test_roadnet_loader.cpp \
  src/low_speed_av_planning/test/test_planning_algorithms.cpp \
  src/low_speed_av_control/include/low_speed_av_control/control_runtime_helpers.hpp \
  src/low_speed_av_control/include/low_speed_av_control/command_smoother.hpp \
  src/low_speed_av_control/src/control_runtime_helpers.cpp \
  src/low_speed_av_control/src/command_smoother.cpp \
  src/low_speed_av_control/src/control_node.cpp \
  src/low_speed_av_control/src/front_ackermann_model.cpp \
  src/low_speed_av_control/src/dual_ackermann_model.cpp \
  src/low_speed_av_control/src/pure_pursuit_controller.cpp \
  src/low_speed_av_control/src/stanley_controller.cpp \
  src/low_speed_av_control/src/lqr_controller.cpp \
  src/low_speed_av_control/src/mpc_sampler_controller.cpp \
  src/low_speed_av_control/src/safety_state_machine.cpp \
  src/low_speed_av_control/test/test_controllers.cpp \
  src/low_speed_av_control/test/test_vehicle_command_pipeline.cpp \
  src/low_speed_av_control/test/test_safety_state_machine.cpp \
  src/low_speed_av_control/test/test_control_runtime_helpers.cpp \
  src/yunle_chassis/chassis_driver/include/chassis_driver/scu_control_frame_builder.hpp \
  src/yunle_chassis/chassis_driver/src/scu_control_frame_builder.cpp \
  src/yunle_chassis/chassis_driver/test/test_chassis_core.cpp
```

只报告格式错误，不自动使用`-i`修改文件。

# 十、定向构建

使用全新的独立目录，避免旧产物污染：

```bash
rm -rf build-humble install-humble log-humble
source /opt/ros/humble/setup.bash
colcon --log-base log-humble build \
  --build-base build-humble \
  --install-base install-humble \
  --symlink-install \
  --event-handlers console_direct+ \
  --packages-up-to \
    low_speed_av_planning \
    low_speed_av_control \
    low_speed_av_bringup \
    chassis_driver
```

如果定向构建失败：

1. 保留完整编译器错误；
2. 按package、target、文件、首个根因分类；
3. 不继续声称后续相关测试PASS；
4. 仍执行不依赖失败target的最大可行测试；
5. 不修改源码。

# 十一、定向production-linked C++测试

定向构建成功后：

```bash
source /opt/ros/humble/setup.bash
source install-humble/setup.bash
colcon --log-base log-humble test \
  --build-base build-humble \
  --install-base install-humble \
  --event-handlers console_direct+ \
  --packages-select \
    low_speed_av_planning \
    low_speed_av_control \
    chassis_driver
colcon test-result --test-result-base build-humble --all --verbose
```

必须从CTest/gtest结果中统计实际运行case，而不是只统计源码：

- Planning：每个target的tests、PASS、FAIL、SKIPPED；
- Control：每个target的tests、PASS、FAIL、SKIPPED；
- Chassis core：active tests和`SKIPPED_KNOWN_PRODUCTION_GAP`分别计数；
- 不得把`GTEST_SKIP()`计为PASS；
- 不得把target编译成功计为test PASS。

# 十二、ROS2 launch/integration测试

执行Bringup测试：

```bash
source /opt/ros/humble/setup.bash
source install-humble/setup.bash
export LOW_SPEED_AV_CHASSIS_TEST_MODE=1
colcon --log-base log-humble test \
  --build-base build-humble \
  --install-base install-humble \
  --event-handlers console_direct+ \
  --packages-select low_speed_av_bringup
colcon test-result --test-result-base build-humble --all --verbose
```

逐项确认实际执行：

### Planning integration

- canonical sample发布ready RoadnetStatus；
- PlanRoute成功；
- task/parking/charging PlanMission成功；
- invalid goal发布failure和emergency trajectory；
- invalid reload fail closed；
- late subscriber/QoS/republish；
- bounded process exit。

### Planning -> Control safety integration

- failure/emergency trajectory进入Control；
- `/control/command`零速、brake、disable；
- SCU command target speed 0、brake enable；
- 不启动真实Chassis Driver。

### Control integration

- `output.mode=both`；
- 四controller切换和READY/reset；
- localization、trajectory、VehicleState timeout；
- autonomous disabled、brake、fault；
- estop latch，普通OK不clear；
- clear service拒绝和成功；
- internal/SCU发布周期与最大间隔；
- late subscriber；
- bounded process exit。

每个失败必须记录test name、timeout、最后状态、topic/service诊断和日志路径。

Chassis软件watchdog相关spec继续标记`SKIPPED_KNOWN_PRODUCTION_GAP`；不要为了获得绿灯删除skip或改变期望。

# 十三、全量构建与全量测试

定向问题处理为“已记录”后，无论定向是否全PASS，都尝试最大可行全量构建。使用另一组干净目录：

```bash
rm -rf build-full install-full log-full
source /opt/ros/humble/setup.bash
colcon --log-base log-full build \
  --build-base build-full \
  --install-base install-full \
  --symlink-install \
  --event-handlers console_direct+
```

全量构建成功后：

```bash
source /opt/ros/humble/setup.bash
source install-full/setup.bash
export LOW_SPEED_AV_CHASSIS_TEST_MODE=1
colcon --log-base log-full test \
  --build-base build-full \
  --install-base install-full \
  --event-handlers console_direct+
colcon test-result --test-result-base build-full --all --verbose
```

报告：

- colcon发现的package总数；
-成功构建package数；
-失败/中止/未处理package数；
-CTest/pytest/launch target总数；
-实际tests总数；
-PASS/FAIL/SKIPPED总数；
-每个失败的首个根因。

# 十四、ASan/UBSan

使用独立目录，只覆盖纯C++ production-linked测试，不启动真实网络节点：

```bash
rm -rf build-asan install-asan log-asan
source /opt/ros/humble/setup.bash
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
colcon --log-base log-asan build \
  --build-base build-asan \
  --install-base install-asan \
  --packages-up-to low_speed_av_planning low_speed_av_control chassis_driver \
  --cmake-args \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
    -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
source install-asan/setup.bash
colcon --log-base log-asan test \
  --build-base build-asan \
  --install-base install-asan \
  --event-handlers console_direct+ \
  --packages-select low_speed_av_planning low_speed_av_control chassis_driver
colcon test-result --test-result-base build-asan --all --verbose
```

如ROS/ASan加载顺序导致工具链问题，保留完整错误并标记`SANITIZER_ENVIRONMENT_BLOCKED`，不能改写为production test失败或PASS。

# 十五、真实硬件隔离复核

执行静态检查：

```bash
grep -R "192\.168\.\|keyboard_scu_control\|chassis_driver_node" -n \
  src/low_speed_av_bringup/test \
  .github/workflows || true
grep -R "create.*socket\|UdpChannel" -n \
  src/yunle_chassis/chassis_driver/test || true
```

确认：

- launch tests没有启动生产Chassis Driver；
- gtest没有打开真实UDP；
- CI没有启动keyboard control；
- CI没有向真实网关发送报文；
- `LOW_SPEED_AV_CHASSIS_TEST_MODE=1`只用于隔离，不被描述成硬件watchdog验证。

如发现可能连接真实设备，立即停止相关测试，标记`BLOCKED_REAL_HARDWARE_RISK`并报告具体文件和启动路径。

# 十六、可选GitHub Actions核验

如果Ubuntu环境已经安装并登录`gh`，执行：

```bash
gh auth status
gh repo view --json nameWithOwner,defaultBranchRef
gh run list --commit "$(git rev-parse HEAD)" --limit 10
```

若存在当前commit的workflow run，等待完成并记录：

```bash
gh run watch <run-id> --exit-status
gh run view <run-id> --log-failed
```

CI状态只能使用：

- `EXECUTED_PASS`；
- `EXECUTED_FAIL`；
- `CONFIGURED_NOT_EXECUTED`；
- `NOT_CONFIGURED`。

本地full PASS不等于GitHub Actions PASS；workflow run属于其他commit也不能用于关闭`CDX-P1-007`。

# 十七、Finding重新判定

依据本次实际证据逐项判断：

- `CDX-P0-001`；
- `CDX-P0-002`；
- `CDX-P1-004`；
- `CDX-P1-005`；
- `CDX-P1-006`；
- `CDX-P1-007`；
- `CDX-P2-003`；
- `CDX-P2-004`；
- `CDX-P2-005`；
- `CDX-P2-006`；
- `CDX-P2-007`；
- `CDX-P2-010`；
- `CDX-P2-011`；
- `CDX-P3-001`；
- `CDX-P3-002`；
- `CDX-P3-003`。

每项写明：

1. 当前状态；
2. production代码证据；
3. 本次实际测试证据；
4. 是否可以关闭；
5. 仍缺少的最小证据。

状态规则：

- production-linked C++和相关ROS2 integration实际PASS后，才能把对应`GENERATED_NOT_EXECUTED`升级；
- 当前commit full CI实际PASS后，`CDX-P1-007`才可标记FIXED；
- `CDX-P0-002`保持`OPEN_SOFTWARE / ACCEPTED_HARDWARE_MITIGATION`；
- 没有真实供应商/bench证据时，硬件watchdog保持`DECLARED_NOT_HIL_VERIFIED / HIL_NOT_EXECUTED`；
- reverse controller能力未实现时，`CDX-P2-007`保持OPEN，但fail-closed测试通过可记录为mitigation verified in software tests；
- 不得用offline Python、源码字符串、未执行test、fake transport或test mode关闭production/HIL finding。

# 十八、结果报告

新增：

```text
reports/ubuntu_humble_full_validation_report.md
```

不要覆盖历史Ubuntu报告。报告必须包含：

```markdown
# Ubuntu Humble Full Validation Report

## Validation identity
- Date/time/timezone
- Repository URL
- Commit SHA
- Branch
- Ubuntu version/kernel
- ROS distro
- Compiler/CMake/Python/colcon versions

## Scope and safety boundaries

## Dependency installation result

## Package and target inventory

## Offline checks

## Directed build result

## Production-linked C++ test result

## ROS2 launch/integration result

## Full build and test result

## ASan/UBSan result

## Hardware isolation audit

## GitHub Actions result

## Finding status

## Failures and root causes

## Generated artifacts and log paths

## Known limitations

## Recommended next action
```

测试统计至少包含：

| Category | Registered | Executed | PASS | FAIL | SKIPPED | Status |
|---|---:|---:|---:|---:|---:|---|
| Offline checks | | | | | | |
| Planning C++ | | | | | | |
| Control C++ | | | | | | |
| Chassis core C++ | | | | | | |
| Planning launch | | | | | | |
| Planning-Control launch | | | | | | |
| Control launch | | | | | | |
| Full colcon | | | | | | |
| Sanitizers | | | | | | |
| GitHub Actions | | | | | | |
| HIL | 0 | 0 | 0 | 0 | 0 | HIL_NOT_EXECUTED |

对每个FAIL附：

- 精确命令；
- 返回码；
- 首个根因；
- 受影响package/target/test；
- 日志路径；
- 是否为production defect、test defect、环境问题或证据缺失；
- 建议的独立修复任务，但本轮不实施。

# 十九、完成前复核

执行：

```bash
git diff --check
git status --short
git diff -- src/low_speed_av_planning src/low_speed_av_control src/yunle_chassis
git diff -- \
  src/low_speed_av_bringup/sample_ad_package \
  roadnet_ad_package_20260610T012525Z_1 \
  roadnet_ad_package_20260610T012525Z_2 \
  templates/sample_ad_package
```

验收要求：

- 除新增验证报告外，production、test、config和fixture没有工作区diff；
- build/install/log目录不加入Git；
- 所有执行过的命令都有结果；
- 未执行项使用准确状态；
- 任何失败不被摘要隐藏；
- HIL没有被伪造为PASS。

# 二十、最终回复格式

最终回复只包含：

1. 验证commit、branch和环境；
2. package/build结果；
3. production-linked C++实际执行数量和PASS/FAIL/SKIPPED；
4. ROS2 launch/integration实际结果；
5. offline、format、sanitizer结果；
6. GitHub Actions实际状态；
7. finding状态变化；
8. 失败根因和日志路径；
9. HIL状态与500 ms边界；
10. 唯一推荐的下一步。

不要自动修复生产代码，不要提交或推送验证报告，除非用户另行明确授权。完成报告后停止。
````

