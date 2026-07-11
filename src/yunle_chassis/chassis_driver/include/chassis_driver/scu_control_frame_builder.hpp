#pragma once

#include "chassis_driver/can_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace chassis_driver {

struct ScuControlFrameInput {
  uint8_t shift_level{1U};
  double front_steering_angle_deg{0.0};
  double rear_steering_angle_deg{0.0};
  double target_speed_kmh{0.0};
  bool brake_enable{false};
  uint8_t left_turn_light_request{0U};
  uint8_t right_turn_light_request{0U};
  uint8_t position_light_request{0U};
  uint8_t low_beam_request{0U};
  uint8_t torque_or_speed_mode{0U};
  bool steering_angle_speed_valid{false};
  bool brake_force_command_valid{false};
};

struct ScuControlFrameLimits {
  double max_steering_angle_deg{27.0};
  double max_target_speed_kmh{15.0};
};

struct ScuControlFrameResult {
  CanFrame frame{};
  bool valid{false};
  std::string rejection_reason;
  std::vector<std::string> warnings;
};

/** Pure production conversion used by both the ROS callback and gtest. */
ScuControlFrameResult
build_scu_control_frame(const ScuControlFrameInput &input,
                        const ScuControlFrameLimits &limits);

} // namespace chassis_driver
