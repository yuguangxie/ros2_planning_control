#include "low_speed_av_control/command_smoother.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_control {

ControlCommand CommandSmoother::smooth(const ControlCommand &target,
                                       const SmootherOptions &options,
                                       double actual_dt_s) {
  diagnostics_ = SmootherDiagnostics{};
  auto out = target;
  if (target.emergency_stop || target.brake > 0.0 || !target.enable) {
    out.speed_mps = 0.0;
    out.acceleration_mps2 = 0.0;
    out.desired_curvature_1pm = 0.0;
    out.steering_angle_rad = 0.0;
    out.front_steering_angle_rad = 0.0;
    out.rear_steering_angle_rad = 0.0;
    out.brake = 1.0;
    diagnostics_.safety_bypass = true;
    previous_ = out;
    previous_acceleration_mps2_ = 0.0;
    initialized_ = true;
    return out;
  }

  const double dt = std::clamp(std::isfinite(actual_dt_s) && actual_dt_s > 0.0
                                   ? actual_dt_s
                                   : options.min_dt_s,
                               options.min_dt_s, options.max_dt_s);
  diagnostics_.applied_dt_s = dt;
  diagnostics_.dt_clamped = !std::isfinite(actual_dt_s) || actual_dt_s <= 0.0 ||
                            std::abs(dt - actual_dt_s) > 1.0e-12;
  if (!initialized_) {
    previous_ = ControlCommand{};
    previous_.enable = true;
    previous_acceleration_mps2_ = 0.0;
    initialized_ = true;
  }

  const double speed_delta = target.speed_mps - previous_.speed_mps;
  const double desired_acceleration = speed_delta / dt;
  double acceleration = std::clamp(
      desired_acceleration, -options.max_decel_mps2, options.max_accel_mps2);
  diagnostics_.speed_limited =
      std::abs(acceleration - desired_acceleration) > 1.0e-12;
  const double jerk_step = options.max_jerk_mps3 * dt;
  const double jerk_limited_acceleration =
      previous_acceleration_mps2_ +
      std::clamp(acceleration - previous_acceleration_mps2_, -jerk_step,
                 jerk_step);
  diagnostics_.jerk_limited =
      std::abs(jerk_limited_acceleration - acceleration) > 1.0e-12;
  acceleration = jerk_limited_acceleration;
  if (speed_delta * acceleration < 0.0) {
    acceleration = 0.0;
  }
  double next_speed = previous_.speed_mps + acceleration * dt;
  if ((speed_delta >= 0.0 && next_speed > target.speed_mps) ||
      (speed_delta < 0.0 && next_speed < target.speed_mps)) {
    next_speed = target.speed_mps;
    acceleration = speed_delta / dt;
  }
  out.speed_mps = next_speed;
  out.acceleration_mps2 = acceleration;
  diagnostics_.applied_acceleration_mps2 = acceleration;

  const double front_step = options.max_front_steer_rate_radps * dt;
  const double rear_step = options.max_rear_steer_rate_radps * dt;
  out.front_steering_angle_rad =
      previous_.front_steering_angle_rad +
      std::clamp(target.front_steering_angle_rad -
                     previous_.front_steering_angle_rad,
                 -front_step, front_step);
  out.rear_steering_angle_rad =
      previous_.rear_steering_angle_rad +
      std::clamp(target.rear_steering_angle_rad -
                     previous_.rear_steering_angle_rad,
                 -rear_step, rear_step);
  diagnostics_.front_steer_rate_limited =
      std::abs(out.front_steering_angle_rad - target.front_steering_angle_rad) >
      1.0e-12;
  diagnostics_.rear_steer_rate_limited =
      std::abs(out.rear_steering_angle_rad - target.rear_steering_angle_rad) >
      1.0e-12;
  out.steering_angle_rad = out.front_steering_angle_rad;
  out.desired_curvature_1pm = target.desired_curvature_1pm;
  previous_ = out;
  previous_acceleration_mps2_ = acceleration;
  return out;
}

void CommandSmoother::reset() {
  previous_ = ControlCommand{};
  previous_acceleration_mps2_ = 0.0;
  initialized_ = false;
  diagnostics_ = SmootherDiagnostics{};
}

} // namespace low_speed_av_control
