#include "low_speed_av_control/front_ackermann_model.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace low_speed_av_control {

ControlCommand FrontAckermannModel::steering_from_curvature(
    double kappa_1pm, const VehicleLimits &limits) const {
  if (!std::isfinite(kappa_1pm) || !std::isfinite(limits.wheel_base_m) ||
      limits.wheel_base_m <= 0.0 ||
      !std::isfinite(limits.max_front_steer_rad) ||
      limits.max_front_steer_rad < 0.0) {
    throw std::invalid_argument(
        "front Ackermann model received invalid curvature or limits");
  }
  ControlCommand cmd;
  cmd.vehicle_model = name();
  // Front Ackermann model: kappa = tan(delta_front) / wheel_base.
  const double delta = std::atan(kappa_1pm * limits.wheel_base_m);
  cmd.front_steering_angle_rad = std::clamp(delta, -limits.max_front_steer_rad,
                                            limits.max_front_steer_rad);
  cmd.rear_steering_angle_rad = 0.0;
  cmd.steering_angle_rad = cmd.front_steering_angle_rad;
  return cmd;
}

} // namespace low_speed_av_control
