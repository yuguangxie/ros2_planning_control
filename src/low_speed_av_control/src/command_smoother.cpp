#include "low_speed_av_control/command_smoother.hpp"

#include <algorithm>

namespace low_speed_av_control {

ControlCommand CommandSmoother::smooth(const ControlCommand & target, const SmootherOptions & options)
{
  auto out = target;
  // Rate-limit speed and steering changes to reduce actuator shock.
  out.speed_mps = previous_.speed_mps + std::clamp(
    target.speed_mps - previous_.speed_mps,
    -options.max_speed_step_mps, options.max_speed_step_mps);
  const double steer_step = options.max_steer_rate_radps * options.dt_s;
  out.front_steering_angle_rad = previous_.front_steering_angle_rad + std::clamp(
    target.front_steering_angle_rad - previous_.front_steering_angle_rad, -steer_step, steer_step);
  out.rear_steering_angle_rad = previous_.rear_steering_angle_rad + std::clamp(
    target.rear_steering_angle_rad - previous_.rear_steering_angle_rad, -steer_step, steer_step);
  out.steering_angle_rad = out.front_steering_angle_rad;
  out.desired_curvature_1pm = target.desired_curvature_1pm;
  if (target.emergency_stop) {
    // Emergency stop overrides normal smoothing so braking is immediate.
    out.speed_mps = 0.0;
    out.desired_curvature_1pm = 0.0;
    out.steering_angle_rad = 0.0;
    out.front_steering_angle_rad = 0.0;
    out.rear_steering_angle_rad = 0.0;
    out.brake = 1.0;
  }
  previous_ = out;
  return out;
}

}  // namespace low_speed_av_control
