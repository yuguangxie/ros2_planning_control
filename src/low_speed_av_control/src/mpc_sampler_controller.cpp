#include "low_speed_av_control/mpc_sampler_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace low_speed_av_control {
namespace {

double normalize_angle(double a)
{
  return std::atan2(std::sin(a), std::cos(a));
}

std::vector<double> curvature_samples(const ControllerOptions & options)
{
  if (!options.mpc_curvature_samples.empty()) {
    return options.mpc_curvature_samples;
  }
  const int count = std::max(1, options.mpc_sample_count);
  if (count == 1) {
    return {0.0};
  }
  std::vector<double> samples;
  samples.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(count - 1);
    samples.push_back(-options.mpc_max_curvature_1pm + 2.0 * options.mpc_max_curvature_1pm * t);
  }
  return samples;
}

}  // namespace

ControlCommand MpcSamplerController::compute(
  const Pose2d & pose,
  const VehicleState & state,
  const Trajectory & trajectory,
  const ControllerOptions & options) const
{
  ControlCommand cmd;
  cmd.controller_algorithm = name();
  if (trajectory.empty()) {
    cmd.enable = false;
    cmd.emergency_stop = true;
    cmd.reason = "empty_trajectory";
    return cmd;
  }
  double best_cost = std::numeric_limits<double>::max();
  for (const double kappa : curvature_samples(options)) {
    Pose2d predicted = pose;
    const int horizon = std::max(1, options.mpc_horizon_steps);
    const double dt = std::max(options.mpc_dt_s, 1e-3);
    for (int step = 0; step < horizon; ++step) {
      predicted.yaw_rad = normalize_angle(predicted.yaw_rad + state.speed_mps * kappa * dt);
      predicted.x_m += std::cos(predicted.yaw_rad) * state.speed_mps * dt;
      predicted.y_m += std::sin(predicted.yaw_rad) * state.speed_mps * dt;
    }
    const auto & ref = trajectory[std::min<std::size_t>(
      trajectory.size() - 1U, static_cast<std::size_t>(horizon - 1))];
    const double dx = predicted.x_m - ref.x_m;
    const double dy = predicted.y_m - ref.y_m;
    const double lateral = -std::sin(ref.yaw_rad) * dx + std::cos(ref.yaw_rad) * dy;
    const double heading = normalize_angle(ref.yaw_rad - predicted.yaw_rad);
    const double speed_error = ref.v_mps - state.speed_mps;
    const double cost =
      options.mpc_lateral_error_weight * std::abs(lateral) +
      options.mpc_heading_error_weight * std::abs(heading) +
      options.mpc_speed_error_weight * std::abs(speed_error) +
      options.mpc_steering_effort_weight * std::abs(kappa);
    if (cost < best_cost) {
      best_cost = cost;
      cmd.desired_curvature_1pm = kappa;
    }
  }
  cmd.steering_angle_rad = std::atan(cmd.desired_curvature_1pm * options.wheel_base_m);
  cmd.speed_mps = trajectory.front().v_mps;
  cmd.gear = trajectory.front().gear;
  cmd.reason = "mpc_sampler_experimental";
  return cmd;
}

}  // namespace low_speed_av_control
