#include "low_speed_av_control/command_limiter.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_control {
namespace {

bool command_is_finite(const ControlCommand & command)
{
  return std::isfinite(command.speed_mps) &&
    std::isfinite(command.acceleration_mps2) &&
    std::isfinite(command.desired_curvature_1pm) &&
    std::isfinite(command.steering_angle_rad) &&
    std::isfinite(command.front_steering_angle_rad) &&
    std::isfinite(command.rear_steering_angle_rad) &&
    std::isfinite(command.brake);
}

}  // namespace

ControlCommand CommandLimiter::limit(const ControlCommand & command, const VehicleLimits & limits) const
{
  auto out = command;
  // Safety guard: a non-finite command must never be published as enabled.
  if (!command_is_finite(out)) {
    out.speed_mps = 0.0;
    out.desired_curvature_1pm = 0.0;
    out.steering_angle_rad = 0.0;
    out.front_steering_angle_rad = 0.0;
    out.rear_steering_angle_rad = 0.0;
    out.acceleration_mps2 = 0.0;
    out.brake = 1.0;
    out.enable = false;
    out.emergency_stop = true;
    out.reason = "nan_or_inf_guard";
  }
  out.speed_mps = std::clamp(out.speed_mps, -limits.max_speed_mps, limits.max_speed_mps);
  // Clamp acceleration and steering to vehicle limits before publishing.
  out.acceleration_mps2 = std::clamp(out.acceleration_mps2, -limits.max_decel_mps2, limits.max_accel_mps2);
  out.front_steering_angle_rad = std::clamp(
    out.front_steering_angle_rad == 0.0 ? out.steering_angle_rad : out.front_steering_angle_rad,
    -limits.max_front_steer_rad, limits.max_front_steer_rad);
  out.rear_steering_angle_rad = std::clamp(
    out.rear_steering_angle_rad, -limits.max_rear_steer_rad, limits.max_rear_steer_rad);
  out.steering_angle_rad = out.front_steering_angle_rad;
  return out;
}

}  // namespace low_speed_av_control
