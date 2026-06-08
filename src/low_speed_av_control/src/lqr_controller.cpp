#include "low_speed_av_control/lqr_controller.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace low_speed_av_control {
namespace {

double normalize_angle(double a)
{
  return std::atan2(std::sin(a), std::cos(a));
}

std::size_t nearest_index(const Pose2d & pose, const Trajectory & trajectory)
{
  std::size_t nearest = 0;
  double best = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < trajectory.size(); ++i) {
    const double d = std::hypot(trajectory[i].x_m - pose.x_m, trajectory[i].y_m - pose.y_m);
    if (d < best) {
      best = d;
      nearest = i;
    }
  }
  return nearest;
}

bool is_stop_trajectory(const Trajectory & trajectory)
{
  return std::all_of(trajectory.begin(), trajectory.end(), [](const auto & point) {
    return std::abs(point.v_mps) < 1.0e-3;
  });
}

std::size_t preview_index(
  const Trajectory & trajectory,
  std::size_t nearest,
  double speed_mps,
  double preview_time_s)
{
  const double preview_distance = std::max(0.0, speed_mps) * std::max(0.0, preview_time_s);
  const double target_s = trajectory[nearest].s_m + preview_distance;
  for (std::size_t i = nearest; i < trajectory.size(); ++i) {
    if (trajectory[i].s_m >= target_s) {
      return i;
    }
  }
  return trajectory.size() - 1U;
}

double finite_or(double value, double fallback)
{
  return std::isfinite(value) ? value : fallback;
}

}  // namespace

ControlCommand LqrController::compute(
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
  if (is_stop_trajectory(trajectory)) {
    cmd.speed_mps = 0.0;
    cmd.enable = false;
    cmd.emergency_stop = true;
    cmd.reason = "stop_trajectory";
    return cmd;
  }

  const std::size_t nearest = nearest_index(pose, trajectory);
  const double nearest_ref_speed = std::abs(trajectory[nearest].v_mps);
  const double raw_speed = std::abs(state.speed_mps) > 1.0e-6 ? std::abs(state.speed_mps) : nearest_ref_speed;
  const double model_speed = std::max(options.lqr_min_speed_mps, finite_or(raw_speed, nearest_ref_speed));
  const auto & ref = trajectory[preview_index(trajectory, nearest, model_speed, options.lqr_preview_time_s)];

  const double dx = pose.x_m - ref.x_m;
  const double dy = pose.y_m - ref.y_m;
  const double e_y = -std::sin(ref.yaw_rad) * dx + std::cos(ref.yaw_rad) * dy;
  const double e_psi = normalize_angle(pose.yaw_rad - ref.yaw_rad);
  const double dt = std::max(options.control_dt_s, 1.0e-3);
  const double wheel_base = std::max(options.wheel_base_m, 1.0e-6);

  const double a00 = 1.0;
  const double a01 = model_speed * dt;
  const double a10 = 0.0;
  const double a11 = 1.0;
  const double b0 = 0.0;
  const double b1 = model_speed * dt / wheel_base;
  const double q0 = std::max(0.0, options.lqr_q_lateral_error);
  const double q1 = std::max(0.0, options.lqr_q_heading_error);
  const double r = std::max(options.lqr_r_steering, 1.0e-9);

  double p00 = q0;
  double p01 = 0.0;
  double p11 = q1;
  for (int iter = 0; iter < std::max(1, options.lqr_max_iterations); ++iter) {
    const double pb0 = p00 * b0 + p01 * b1;
    const double pb1 = p01 * b0 + p11 * b1;
    const double denom = std::max(r + b0 * pb0 + b1 * pb1, 1.0e-12);

    const double pa00 = p00 * a00 + p01 * a10;
    const double pa01 = p00 * a01 + p01 * a11;
    const double pa10 = p01 * a00 + p11 * a10;
    const double pa11 = p01 * a01 + p11 * a11;

    const double atpa00 = a00 * pa00 + a10 * pa10;
    const double atpa01 = a00 * pa01 + a10 * pa11;
    const double atpa11 = a01 * pa01 + a11 * pa11;

    const double atpb0 = a00 * pb0 + a10 * pb1;
    const double atpb1 = a01 * pb0 + a11 * pb1;
    const double p_next00 = atpa00 - atpb0 * atpb0 / denom + q0;
    const double p_next01 = atpa01 - atpb0 * atpb1 / denom;
    const double p_next11 = atpa11 - atpb1 * atpb1 / denom + q1;

    const double diff = std::max({
      std::abs(p_next00 - p00),
      std::abs(p_next01 - p01),
      std::abs(p_next11 - p11)});
    p00 = p_next00;
    p01 = p_next01;
    p11 = p_next11;
    if (diff < options.lqr_convergence_eps) {
      break;
    }
  }

  const double pb0 = p00 * b0 + p01 * b1;
  const double pb1 = p01 * b0 + p11 * b1;
  const double denom = std::max(r + b0 * pb0 + b1 * pb1, 1.0e-12);
  const double pa00 = p00 * a00 + p01 * a10;
  const double pa01 = p00 * a01 + p01 * a11;
  const double pa10 = p01 * a00 + p11 * a10;
  const double pa11 = p01 * a01 + p11 * a11;
  const double k0 = (b0 * pa00 + b1 * pa10) / denom;
  const double k1 = (b0 * pa01 + b1 * pa11) / denom;

  const double delta_ff = options.lqr_use_curvature_feedforward ?
    std::atan(wheel_base * ref.kappa_1pm) : 0.0;
  const double delta_fb = -(k0 * e_y + k1 * e_psi);
  const double delta_cmd = std::clamp(
    delta_ff + delta_fb,
    -std::abs(options.lqr_max_steering_angle_rad),
    std::abs(options.lqr_max_steering_angle_rad));

  cmd.steering_angle_rad = delta_cmd;
  cmd.desired_curvature_1pm = std::tan(delta_cmd) / wheel_base;
  cmd.speed_mps = ref.v_mps;
  cmd.gear = ref.gear;
  cmd.reason = "lqr_tracking";
  return cmd;
}

}  // namespace low_speed_av_control
