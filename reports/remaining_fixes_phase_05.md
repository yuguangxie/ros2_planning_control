# Remaining Fixes Phase 05 Report

- 目标：提升 LQR/MPC sampler 配置成熟度。
- 变更文件：`src/low_speed_av_control/include/low_speed_av_control/controller_base.hpp`、`src/low_speed_av_control/src/lqr_controller.cpp`、`src/low_speed_av_control/src/mpc_sampler_controller.cpp`、`src/low_speed_av_control/src/control_node.cpp`、`src/low_speed_av_control/config/control_params.yaml`、`src/low_speed_av_bringup/config/control_params.yaml`、`src/low_speed_av_control/README.md`。
- 已处理审计发现：A2-R-005、F-CT-006、A2-CT-004。
- AD Package 兼容性：控制模块仍不直接读取 AD Package，只消费规划轨迹。
- 安全影响：LQR/MPC 仍标记 experimental，但现在配置改变会影响输出，降低“配置看似生效实际无效”的风险。
- 测试/检查：`offline_remaining_fixes_smoke.py` 验证 LQR/MPC 配置变化会改变输出。
- SKIPPED_ROS2_UNAVAILABLE：`ros2 topic pub /planning/trajectory ...`、`ros2 topic echo /control/command`、`colcon build --packages-select low_speed_av_control`。
- 剩余限制：LQR 不是完整状态空间求解器；MPC sampler 无 heavy solver 和完整车辆模型。
- 下一步：补 C++ 单元测试和真实车辆模型预测。

