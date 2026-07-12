#include "low_speed_av_control/control_runtime_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>

namespace low_speed_av_control {
namespace {

void require_positive(const std::string &name, double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    throw std::invalid_argument(name + " must be finite and > 0");
  }
}

void require_nonnegative(const std::string &name, double value) {
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(name + " must be finite and >= 0");
  }
}

bool finite_pose(const Pose2d &pose) {
  return std::isfinite(pose.x_m) && std::isfinite(pose.y_m) &&
         std::isfinite(pose.yaw_rad);
}

bool finite_state(const VehicleState &state) {
  return std::isfinite(state.speed_mps) &&
         std::isfinite(state.acceleration_mps2) &&
         std::isfinite(state.front_steering_angle_rad) &&
         std::isfinite(state.rear_steering_angle_rad);
}

double heading_error(double vehicle_yaw, double reference_yaw) {
  return std::fabs(std::atan2(std::sin(vehicle_yaw - reference_yaw),
                              std::cos(vehicle_yaw - reference_yaw)));
}

} // namespace

void validate_control_configuration(const ControlConfiguration &config) {
  if (config.output_mode != "internal" &&
      config.output_mode != "scu_control_command" &&
      config.output_mode != "both") {
    throw std::invalid_argument(
        "output.mode must be internal, scu_control_command, or both");
  }
  const std::set<std::string> controllers{"pure_pursuit", "stanley", "lqr",
                                          "mpc_sampler"};
  const std::set<std::string> models{"front_ackermann", "dual_ackermann"};
  if (controllers.count(config.controller_algorithm) == 0U) {
    throw std::invalid_argument("unsupported controller.algorithm: " +
                                config.controller_algorithm);
  }
  if (models.count(config.vehicle_model) == 0U) {
    throw std::invalid_argument("unsupported vehicle.model: " +
                                config.vehicle_model);
  }
  if (config.allowed_trajectory_statuses.empty()) {
    throw std::invalid_argument(
        "controller.allowed_trajectory_statuses must not be empty");
  }
  const std::set<std::string> supported_normal_statuses{"ok"};
  std::set<std::string> statuses;
  for (const auto &status : config.allowed_trajectory_statuses) {
    if (supported_normal_statuses.count(status) == 0U) {
      throw std::invalid_argument(
          "controller.allowed_trajectory_statuses contains unsupported normal "
          "status: " +
          status);
    }
    if (!statuses.insert(status).second) {
      throw std::invalid_argument(
          "allowed trajectory statuses must be non-empty and unique");
    }
  }

  require_nonnegative("controller.trajectory_s_tolerance_m",
                      config.trajectory_s_tolerance_m);
  require_nonnegative("safety.clear_speed_threshold_mps",
                      config.clear_speed_threshold_mps);
  require_positive("controller.control_rate_hz", config.timing.control_rate_hz);
  require_positive("controller.localization_timeout_s",
                   config.timing.localization_timeout_s);
  require_positive("controller.trajectory_timeout_s",
                   config.timing.trajectory_timeout_s);
  require_positive("vehicle_state.timeout_s",
                   config.timing.vehicle_state_timeout_s);
  require_positive("control.status_publish_rate_hz",
                   config.timing.status_publish_rate_hz);
  require_positive("control.publish_deadline_warning_s",
                   config.timing.publish_deadline_warning_s);
  require_positive("hardware_watchdog.timeout_s",
                   config.timing.hardware_watchdog_timeout_s);
  if (config.timing.hardware_watchdog_timeout_s > 0.5) {
    throw std::invalid_argument(
        "hardware_watchdog.timeout_s must not exceed the declared 0.5 s "
        "hardware limit");
  }
  if (1.0 / config.timing.control_rate_hz >=
      config.timing.publish_deadline_warning_s) {
    throw std::invalid_argument(
        "publish deadline warning must exceed the nominal control period");
  }
  if (config.timing.publish_deadline_warning_s >=
      config.timing.hardware_watchdog_timeout_s) {
    throw std::invalid_argument(
        "publish deadline warning must remain below hardware watchdog timeout");
  }
  if (config.timing.cadence_window_size < 4U ||
      config.timing.cadence_window_size > 4096U) {
    throw std::invalid_argument(
        "control.cadence_window_size must be in [4, 4096]");
  }
  if (config.timing.hardware_watchdog_contract_status !=
          "DECLARED_NOT_HIL_VERIFIED" &&
      config.timing.hardware_watchdog_contract_status != "HIL_VERIFIED") {
    throw std::invalid_argument("hardware_watchdog.contract_status is invalid");
  }

  const auto &limits = config.vehicle_limits;
  require_positive("vehicle.wheel_base_m", limits.wheel_base_m);
  require_nonnegative("vehicle.max_speed_mps", limits.max_speed_mps);
  require_nonnegative("vehicle.max_accel_mps2", limits.max_accel_mps2);
  require_nonnegative("vehicle.max_decel_mps2", limits.max_decel_mps2);
  require_nonnegative("vehicle.max_front_steer_rad",
                      limits.max_front_steer_rad);
  require_nonnegative("vehicle.max_rear_steer_rad", limits.max_rear_steer_rad);
  require_nonnegative("vehicle.max_front_steer_rate_radps",
                      limits.max_front_steer_rate_radps);
  require_nonnegative("vehicle.max_rear_steer_rate_radps",
                      limits.max_rear_steer_rate_radps);
  require_nonnegative("vehicle.rear_steer_ratio", limits.rear_steer_ratio);
  if (limits.rear_steer_ratio > 1.0) {
    throw std::invalid_argument("vehicle.rear_steer_ratio must be <= 1.0");
  }

  const auto &options = config.controller_options;
  require_positive("pure_pursuit.lookahead_min_m", options.lookahead_min_m);
  require_positive("pure_pursuit.lookahead_max_m", options.lookahead_max_m);
  if (options.lookahead_min_m > options.lookahead_max_m) {
    throw std::invalid_argument(
        "pure pursuit minimum lookahead exceeds maximum");
  }
  require_nonnegative("pure_pursuit.lookahead_speed_gain",
                      options.lookahead_speed_gain);
  require_nonnegative("stanley.k", options.stanley_k);
  require_positive("stanley.epsilon_mps", options.stanley_epsilon_mps);
  require_nonnegative("stanley.max_correction_rad", options.max_correction_rad);
  require_positive("controller.control_dt_s", options.control_dt_s);
  require_nonnegative("lqr.q_lateral_error", options.lqr_q_lateral_error);
  require_nonnegative("lqr.q_heading_error", options.lqr_q_heading_error);
  require_positive("lqr.r_steering", options.lqr_r_steering);
  if (options.lqr_max_iterations <= 0) {
    throw std::invalid_argument("lqr.max_iterations must be > 0");
  }
  require_positive("lqr.convergence_eps", options.lqr_convergence_eps);
  require_positive("lqr.min_speed_mps", options.lqr_min_speed_mps);
  require_nonnegative("lqr.preview_time_s", options.lqr_preview_time_s);
  require_positive("lqr.max_steering_angle_rad",
                   options.lqr_max_steering_angle_rad);
  if (options.mpc_horizon_steps <= 0) {
    throw std::invalid_argument("mpc_sampler.horizon_steps must be > 0");
  }
  require_positive("mpc_sampler.dt_s", options.mpc_dt_s);
  if (options.mpc_curvature_samples.empty()) {
    throw std::invalid_argument(
        "mpc_sampler.curvature_samples must not be empty");
  }
  for (const auto sample : options.mpc_curvature_samples) {
    if (!std::isfinite(sample)) {
      throw std::invalid_argument(
          "mpc_sampler.curvature_samples contains non-finite value");
    }
  }
  require_nonnegative("mpc_sampler.lateral_error_weight",
                      options.mpc_lateral_error_weight);
  require_nonnegative("mpc_sampler.heading_error_weight",
                      options.mpc_heading_error_weight);
  require_nonnegative("mpc_sampler.speed_error_weight",
                      options.mpc_speed_error_weight);
  require_nonnegative("mpc_sampler.steering_effort_weight",
                      options.mpc_steering_effort_weight);

  const auto &smoother = config.smoother_options;
  require_nonnegative("command_smoother.max_accel_mps2",
                      smoother.max_accel_mps2);
  require_nonnegative("command_smoother.max_decel_mps2",
                      smoother.max_decel_mps2);
  require_nonnegative("command_smoother.max_jerk_mps3", smoother.max_jerk_mps3);
  require_nonnegative("vehicle.max_front_steer_rate_radps",
                      smoother.max_front_steer_rate_radps);
  require_nonnegative("vehicle.max_rear_steer_rate_radps",
                      smoother.max_rear_steer_rate_radps);
  require_positive("command_smoother.min_dt_s", smoother.min_dt_s);
  require_positive("command_smoother.max_dt_s", smoother.max_dt_s);
  if (smoother.min_dt_s > smoother.max_dt_s) {
    throw std::invalid_argument("command smoother min dt exceeds max dt");
  }

  require_positive("scu.max_steering_angle_deg",
                   config.scu_options.max_steering_angle_deg);
  require_nonnegative("scu.max_target_speed_kmh",
                      config.scu_options.max_target_speed_kmh);
  if (std::abs(std::abs(config.scu_options.front_steer_sign) - 1.0) > 1.0e-9 ||
      std::abs(std::abs(config.scu_options.rear_steer_sign) - 1.0) > 1.0e-9) {
    throw std::invalid_argument("SCU steering signs must be +1 or -1");
  }
  if (config.scu_options.overrange_policy != "clamp" &&
      config.scu_options.overrange_policy != "zero") {
    throw std::invalid_argument("scu.overrange_policy must be clamp or zero");
  }
  if (config.scu_options.stop_shift_level < 1U ||
      config.scu_options.stop_shift_level > 3U) {
    throw std::invalid_argument("scu.stop_shift_level must be 1, 2, or 3");
  }
  if (config.scu_options.torque_or_speed_mode > 1U) {
    throw std::invalid_argument("scu.torque_or_speed_mode must be 0 or 1");
  }
  for (const auto light :
       {config.scu_options.left_turn_light, config.scu_options.right_turn_light,
        config.scu_options.position_light, config.scu_options.low_beam}) {
    if (light > 1U) {
      throw std::invalid_argument("SCU light request must be 0 or 1");
    }
  }

  if (config.progress.forward_window_points == 0U) {
    throw std::invalid_argument(
        "controller.progress_forward_window_points must be > 0");
  }
  require_positive("controller.progress_max_heading_error_rad",
                   config.progress.max_heading_error_rad);
}

ControllerInputDecision
validate_controller_input(const Pose2d &pose, const VehicleState &state,
                          const Trajectory &trajectory,
                          const ControllerOptions &options) {
  if (!finite_pose(pose)) {
    return {false, "invalid_controller_pose"};
  }
  if (!finite_state(state)) {
    return {false, "invalid_controller_vehicle_state"};
  }
  if (state.speed_mps < -1.0e-3) {
    return {false, "unexpected_negative_vehicle_speed"};
  }
  if (state.gear == 2) {
    return {false, "unsupported_reverse_vehicle_state"};
  }
  if (state.gear != 1) {
    return {false, "vehicle_state_not_in_drive"};
  }
  if (trajectory.empty()) {
    return {false, "empty_trajectory"};
  }
  if (!std::isfinite(options.wheel_base_m) || options.wheel_base_m <= 0.0 ||
      !std::isfinite(options.control_dt_s) || options.control_dt_s <= 0.0) {
    return {false, "invalid_controller_options"};
  }
  bool requests_motion = false;
  for (std::size_t i = 0U; i < trajectory.size(); ++i) {
    const auto &point = trajectory[i];
    if (!std::isfinite(point.x_m) || !std::isfinite(point.y_m) ||
        !std::isfinite(point.yaw_rad) || !std::isfinite(point.kappa_1pm) ||
        !std::isfinite(point.s_m) || !std::isfinite(point.v_mps)) {
      return {false,
              "invalid_controller_trajectory_point:" + std::to_string(i)};
    }
    if (point.gear == 2) {
      return {false, "unsupported_reverse_tracking"};
    }
    if (point.gear != 1) {
      return {false,
              "unsupported_controller_gear:" + std::to_string(point.gear)};
    }
    if (point.v_mps < 0.0) {
      return {false,
              "negative_controller_trajectory_speed:" + std::to_string(i)};
    }
    requests_motion = requests_motion || std::abs(point.v_mps) >= 1.0e-3;
  }
  if (!requests_motion) {
    return {false, "stop_trajectory"};
  }
  return {true, "tracking_input_valid"};
}

ControlCommand controller_stop_command(const std::string &controller,
                                       const std::string &reason) {
  ControlCommand command;
  command.controller_algorithm = controller;
  command.speed_mps = 0.0;
  command.acceleration_mps2 = 0.0;
  command.brake = 1.0;
  command.enable = false;
  command.emergency_stop = false;
  command.reason = reason.empty() ? "controller_input_rejected" : reason;
  return command;
}

void TrackingProgressTracker::reset() {
  trajectory_identity_.clear();
  progress_index_ = 0U;
  initialized_ = false;
}

Trajectory
TrackingProgressTracker::select_window(const Trajectory &trajectory,
                                       const Pose2d &pose, int vehicle_gear,
                                       const std::string &trajectory_identity,
                                       const TrackingProgressOptions &options) {
  if (trajectory.empty()) {
    reset();
    return {};
  }
  if (!initialized_ || trajectory_identity != trajectory_identity_ ||
      progress_index_ >= trajectory.size()) {
    trajectory_identity_ = trajectory_identity;
    progress_index_ = 0U;
    initialized_ = true;
  }
  const std::size_t begin =
      progress_index_ > options.backward_window_points
          ? progress_index_ - options.backward_window_points
          : 0U;
  const std::size_t remaining = trajectory.size() - progress_index_ - 1U;
  const std::size_t forward =
      std::min(options.forward_window_points, remaining);
  const std::size_t end = progress_index_ + forward + 1U;
  std::size_t best_index = progress_index_;
  double best_distance = std::numeric_limits<double>::infinity();
  for (std::size_t i = begin; i < end; ++i) {
    if (vehicle_gear >= 1 && vehicle_gear <= 2 &&
        trajectory[i].gear != vehicle_gear) {
      continue;
    }
    if (heading_error(pose.yaw_rad, trajectory[i].yaw_rad) >
        options.max_heading_error_rad) {
      continue;
    }
    const double distance =
        std::hypot(trajectory[i].x_m - pose.x_m, trajectory[i].y_m - pose.y_m);
    if (distance < best_distance) {
      best_distance = distance;
      best_index = i;
    }
  }
  progress_index_ = std::max(progress_index_, best_index);
  const std::size_t local_end = std::min(
      trajectory.size(), progress_index_ + options.forward_window_points + 1U);
  return Trajectory(
      trajectory.begin() + static_cast<std::ptrdiff_t>(progress_index_),
      trajectory.begin() + static_cast<std::ptrdiff_t>(local_end));
}

ControlCadenceMonitor::ControlCadenceMonitor(
    const ControlTimingOptions &options) {
  reset(options);
}

void ControlCadenceMonitor::reset(const ControlTimingOptions &options) {
  options_ = options;
  intervals_.clear();
  last_cycle_time_s_ = 0.0;
  last_publish_time_s_ = 0.0;
  last_interval_s_ = 0.0;
  max_interval_s_ = 0.0;
  cycle_count_ = 0U;
  publish_count_ = 0U;
  missed_cycles_ = 0U;
  deadline_misses_ = 0U;
  initialized_ = false;
}

double ControlCadenceMonitor::observe_cycle(double steady_now_s) {
  const double expected = 1.0 / options_.control_rate_hz;
  ++cycle_count_;
  if (!std::isfinite(steady_now_s) || !initialized_) {
    last_cycle_time_s_ = steady_now_s;
    initialized_ = std::isfinite(steady_now_s);
    last_interval_s_ = expected;
    return expected;
  }
  const double interval = steady_now_s - last_cycle_time_s_;
  last_cycle_time_s_ = steady_now_s;
  last_interval_s_ =
      std::isfinite(interval) && interval > 0.0 ? interval : expected;
  max_interval_s_ = std::max(max_interval_s_, last_interval_s_);
  intervals_.push_back(last_interval_s_);
  if (intervals_.size() > options_.cadence_window_size) {
    intervals_.erase(intervals_.begin());
  }
  if (last_interval_s_ > expected * 1.5) {
    const auto elapsed_periods =
        static_cast<uint64_t>(std::floor(last_interval_s_ / expected));
    missed_cycles_ += elapsed_periods > 0U ? elapsed_periods - 1U : 0U;
  }
  if (last_interval_s_ > options_.publish_deadline_warning_s) {
    ++deadline_misses_;
  }
  return last_interval_s_;
}

void ControlCadenceMonitor::mark_publish(double steady_now_s) {
  if (std::isfinite(steady_now_s)) {
    last_publish_time_s_ = steady_now_s;
  }
  ++publish_count_;
}

CadenceSnapshot ControlCadenceMonitor::snapshot(double steady_now_s) const {
  CadenceSnapshot result;
  result.last_interval_s = last_interval_s_;
  result.max_interval_s = max_interval_s_;
  result.cycle_count = cycle_count_;
  result.publish_count = publish_count_;
  result.missed_cycles = missed_cycles_;
  result.deadline_misses = deadline_misses_;
  result.hardware_timeout_gap =
      last_interval_s_ >= options_.hardware_watchdog_timeout_s;
  if (last_publish_time_s_ > 0.0 && steady_now_s >= last_publish_time_s_) {
    result.last_publish_age_s = steady_now_s - last_publish_time_s_;
  }
  if (!intervals_.empty()) {
    auto sorted = intervals_;
    std::sort(sorted.begin(), sorted.end());
    const auto index = static_cast<std::size_t>(
        std::ceil(0.95 * static_cast<double>(sorted.size())) - 1.0);
    result.p95_interval_s = sorted[std::min(index, sorted.size() - 1U)];
  }
  return result;
}

} // namespace low_speed_av_control
