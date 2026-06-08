#include "low_speed_av_control/front_ackermann_model.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_control {

ControlCommand FrontAckermannModel::steering_from_curvature(double kappa_1pm, const VehicleLimits & limits) const
{
  ControlCommand cmd;
  cmd.vehicle_model = name();
  // Front Ackermann model: kappa = tan(delta_front) / wheel_base.
  const double delta = std::atan(kappa_1pm * limits.wheel_base_m);
  cmd.front_steering_angle_rad = std::clamp(delta, -limits.max_front_steer_rad, limits.max_front_steer_rad);
  cmd.rear_steering_angle_rad = 0.0;
  cmd.steering_angle_rad = cmd.front_steering_angle_rad;
  return cmd;
}

}  // namespace low_speed_av_control
