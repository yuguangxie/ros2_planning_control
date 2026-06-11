#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace low_speed_av_control {

// ROS-independent pose type used by controllers and offline tests.
struct Pose2d {
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
};

// Minimal trajectory point consumed by tracking controllers.
struct TrajectoryPoint {
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
  double kappa_1pm{0.0};
  double s_m{0.0};
  double v_mps{0.0};
  int gear{1};
};

// Vehicle state input. Missing runtime values should default to conservative
// zero-speed behavior.
struct VehicleState {
  double speed_mps{0.0};
  double acceleration_mps2{0.0};
  double front_steering_angle_rad{0.0};
  double rear_steering_angle_rad{0.0};
  int gear{1};
  bool autonomous_enabled{true};
};

// Internal command before conversion to low_speed_av_interfaces/ControlCommand.
struct ControlCommand {
  double speed_mps{0.0};
  double acceleration_mps2{0.0};
  double desired_curvature_1pm{0.0};
  double steering_angle_rad{0.0};
  double front_steering_angle_rad{0.0};
  double rear_steering_angle_rad{0.0};
  double brake{0.0};
  int gear{1};
  bool enable{true};
  bool emergency_stop{false};
  std::string controller_algorithm;
  std::string vehicle_model;
  std::string reason;
};

struct ScuCommandOptions {
  double max_steering_angle_deg{27.0};
  double max_target_speed_kmh{5.0};
  double front_steer_sign{1.0};
  double rear_steer_sign{1.0};
  std::string overrange_policy{"clamp"};
  uint8_t stop_shift_level{1};
  uint8_t torque_or_speed_mode{1};
  bool steering_angle_speed_valid{false};
  bool brake_force_command_valid{false};
  uint8_t left_turn_light{0};
  uint8_t right_turn_light{0};
  uint8_t position_light{0};
  uint8_t low_beam{0};
};

// Physical and command limits for low-speed Ackermann operation.
struct VehicleLimits {
  double wheel_base_m{1.2};
  double max_speed_mps{1.2};
  double max_accel_mps2{0.5};
  double max_decel_mps2{0.8};
  double max_front_steer_rad{0.6};
  double max_rear_steer_rad{0.6};
  double max_front_steer_rate_radps{0.5};
  double max_rear_steer_rate_radps{0.5};
  double rear_steer_ratio{0.5};
};

using Trajectory = std::vector<TrajectoryPoint>;

}  // namespace low_speed_av_control
