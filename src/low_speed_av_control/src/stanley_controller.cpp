#include "low_speed_av_control/stanley_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "low_speed_av_control/control_runtime_helpers.hpp"

namespace low_speed_av_control {
namespace {

double normalize_angle(double a) {
  return std::atan2(std::sin(a), std::cos(a));
}

std::size_t nearest_index(const Pose2d &pose, const Trajectory &trajectory) {
  std::size_t nearest = 0;
  double best = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < trajectory.size(); ++i) {
    const double d =
        std::hypot(trajectory[i].x_m - pose.x_m, trajectory[i].y_m - pose.y_m);
    if (d < best) {
      best = d;
      nearest = i;
    }
  }
  return nearest;
}

} // namespace

ControlCommand
StanleyController::compute(const Pose2d &pose, const VehicleState &state,
                           const Trajectory &trajectory,
                           const ControllerOptions &options) const {
  const auto input =
      validate_controller_input(pose, state, trajectory, options);
  if (!input.valid) {
    return controller_stop_command(name(), input.reason);
  }
  ControlCommand cmd;
  cmd.controller_algorithm = name();
  const auto i = nearest_index(pose, trajectory);
  const auto &ref = trajectory[i];
  const double heading_error = normalize_angle(ref.yaw_rad - pose.yaw_rad);
  const double dx = pose.x_m - ref.x_m;
  const double dy = pose.y_m - ref.y_m;
  // Signed lateral error in the reference heading frame.
  const double lateral =
      -std::sin(ref.yaw_rad) * dx + std::cos(ref.yaw_rad) * dy;
  const double correction = std::clamp(
      std::atan2(options.stanley_k * lateral,
                 std::abs(state.speed_mps) + options.stanley_epsilon_mps),
      -options.max_correction_rad, options.max_correction_rad);
  const double steering = normalize_angle(heading_error + correction);
  cmd.steering_angle_rad = steering;
  cmd.desired_curvature_1pm =
      std::tan(steering) / std::max(options.wheel_base_m, 1e-6);
  cmd.speed_mps = ref.v_mps;
  cmd.gear = ref.gear;
  return cmd;
}

} // namespace low_speed_av_control
