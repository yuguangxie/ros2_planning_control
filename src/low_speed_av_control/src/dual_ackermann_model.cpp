#include "low_speed_av_control/dual_ackermann_model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace low_speed_av_control {

ControlCommand
DualAckermannModel::steering_from_curvature(double kappa_1pm,
                                            const VehicleLimits &limits) const {
  if (!std::isfinite(kappa_1pm) || !std::isfinite(limits.wheel_base_m) ||
      limits.wheel_base_m <= 0.0 || !std::isfinite(limits.rear_steer_ratio) ||
      limits.rear_steer_ratio < 0.0 || limits.rear_steer_ratio > 1.0 ||
      !std::isfinite(limits.max_front_steer_rad) ||
      limits.max_front_steer_rad < 0.0 ||
      !std::isfinite(limits.max_rear_steer_rad) ||
      limits.max_rear_steer_rad < 0.0) {
    throw std::invalid_argument(
        "dual Ackermann model received invalid curvature or limits");
  }
  ControlCommand cmd;
  cmd.vehicle_model = name();
  // Counter-phase dual Ackermann:
  // tan(delta_front) = kappa * wheel_base / (1 + rear_steer_ratio)
  // tan(delta_rear) = -rear_steer_ratio * tan(delta_front)
  const double ratio = limits.rear_steer_ratio;
  const double tan_front = kappa_1pm * limits.wheel_base_m / (1.0 + ratio);
  const double delta_front = std::atan(tan_front);
  const double delta_rear = std::atan(-ratio * tan_front);
  cmd.front_steering_angle_rad = std::clamp(
      delta_front, -limits.max_front_steer_rad, limits.max_front_steer_rad);
  cmd.rear_steering_angle_rad = std::clamp(
      delta_rear, -limits.max_rear_steer_rad, limits.max_rear_steer_rad);
  cmd.steering_angle_rad = cmd.front_steering_angle_rad;
  return cmd;
}

} // namespace low_speed_av_control
