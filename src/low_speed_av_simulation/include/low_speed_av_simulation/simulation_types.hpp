#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace low_speed_av_simulation {

struct PlantState {
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
  double speed_mps{0.0};
  double acceleration_mps2{0.0};
  double front_steering_angle_rad{0.0};
  double rear_steering_angle_rad{0.0};
  int gear{1};
};

struct PlantCommand {
  double target_speed_mps{0.0};
  double target_acceleration_mps2{0.0};
  double front_steering_angle_rad{0.0};
  double rear_steering_angle_rad{0.0};
  double brake{0.0};
  int gear{1};
  bool enable{false};
  bool emergency_stop{false};
  bool command_timed_out{false};
};

struct PlantOptions {
  double wheel_base_m{1.2};
  double min_dt_s{0.001};
  double max_dt_s{0.05};
  double max_speed_mps{1.0};
  double max_acceleration_mps2{0.5};
  double max_deceleration_mps2{0.8};
  double max_jerk_mps3{2.0};
  double max_front_steering_angle_rad{0.6};
  double max_rear_steering_angle_rad{0.6};
  double max_front_steering_rate_radps{0.8};
  double max_rear_steering_rate_radps{0.8};
  double brake_deceleration_mps2{1.0};
  double emergency_deceleration_mps2{1.5};
  double goal_tolerance_m{0.3};
  double yaw_tolerance_rad{0.35};
};

struct PlantStepResult {
  bool command_valid{true};
  bool stopped{false};
  std::string stop_reason{"none"};
};

struct RuntimeMetrics {
  double step_interval_max_s{0.0};
  double step_interval_p95_s{0.0};
  double localization_interval_max_s{0.0};
  double localization_interval_p95_s{0.0};
  double control_interval_max_s{0.0};
  double control_interval_p95_s{0.0};
  double lateral_error_max_m{0.0};
  double lateral_error_rms_m{0.0};
  double lateral_error_p95_m{0.0};
  double heading_error_max_rad{0.0};
  double heading_error_rms_rad{0.0};
  double heading_error_p95_rad{0.0};
  double goal_distance_m{0.0};
  double goal_yaw_error_rad{0.0};
  std::size_t timeout_count{0U};
  std::size_t non_finite_count{0U};
  std::size_t step_count{0U};
  std::size_t localization_publish_count{0U};
  std::size_t control_command_count{0U};
};

} // namespace low_speed_av_simulation
