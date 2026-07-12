#include "low_speed_av_control/pure_pursuit_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "low_speed_av_control/control_runtime_helpers.hpp"

namespace low_speed_av_control {
namespace {

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
PurePursuitController::compute(const Pose2d &pose, const VehicleState &state,
                               const Trajectory &trajectory,
                               const ControllerOptions &options) const {
  const auto input =
      validate_controller_input(pose, state, trajectory, options);
  if (!input.valid) {
    return controller_stop_command(name(), input.reason);
  }
  ControlCommand cmd;
  cmd.controller_algorithm = name();
  const auto nearest = nearest_index(pose, trajectory);
  // Increase lookahead with speed while keeping a low-speed bounded range.
  const double lookahead =
      std::clamp(options.lookahead_min_m +
                     std::abs(state.speed_mps) * options.lookahead_speed_gain,
                 options.lookahead_min_m, options.lookahead_max_m);
  const double target_s = trajectory[nearest].s_m + lookahead;
  auto target = trajectory.back();
  for (std::size_t i = nearest; i < trajectory.size(); ++i) {
    if (trajectory[i].s_m >= target_s) {
      target = trajectory[i];
      break;
    }
  }
  const double dx = target.x_m - pose.x_m;
  const double dy = target.y_m - pose.y_m;
  // Transform target point into the vehicle frame and compute steering demand.
  const double y_vehicle =
      -std::sin(pose.yaw_rad) * dx + std::cos(pose.yaw_rad) * dy;
  cmd.desired_curvature_1pm =
      2.0 * y_vehicle / std::max(lookahead * lookahead, 1e-6);
  cmd.steering_angle_rad =
      std::atan(cmd.desired_curvature_1pm * options.wheel_base_m);
  cmd.speed_mps = target.v_mps;
  cmd.gear = target.gear;
  return cmd;
}

} // namespace low_speed_av_control
