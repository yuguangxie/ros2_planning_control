#include "low_speed_av_simulation/kinematic_vehicle_plant.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace low_speed_av_simulation {
namespace {

constexpr double kPi = 3.14159265358979323846;

double normalize_angle(double angle) {
  return std::remainder(angle, 2.0 * kPi);
}

bool finite_state(const PlantState &state) {
  return std::isfinite(state.x_m) && std::isfinite(state.y_m) &&
         std::isfinite(state.yaw_rad) && std::isfinite(state.speed_mps) &&
         std::isfinite(state.acceleration_mps2) &&
         std::isfinite(state.front_steering_angle_rad) &&
         std::isfinite(state.rear_steering_angle_rad);
}

bool finite_command(const PlantCommand &command) {
  return std::isfinite(command.target_speed_mps) &&
         std::isfinite(command.target_acceleration_mps2) &&
         std::isfinite(command.front_steering_angle_rad) &&
         std::isfinite(command.rear_steering_angle_rad) &&
         std::isfinite(command.brake);
}

double move_toward(double current, double target, double max_delta) {
  return current + std::clamp(target - current, -max_delta, max_delta);
}

void require_positive_finite(double value, const char *name) {
  if (!std::isfinite(value) || value <= 0.0) {
    throw std::invalid_argument(std::string(name) + " must be finite and > 0");
  }
}

void require_nonnegative_finite(double value, const char *name) {
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(std::string(name) + " must be finite and >= 0");
  }
}

} // namespace

KinematicVehiclePlant::KinematicVehiclePlant(const PlantOptions &options)
    : options_(options) {
  validate_options();
  reset();
}

void KinematicVehiclePlant::validate_options() const {
  require_positive_finite(options_.wheel_base_m, "wheel_base_m");
  require_positive_finite(options_.min_dt_s, "min_dt_s");
  require_positive_finite(options_.max_dt_s, "max_dt_s");
  if (options_.max_dt_s < options_.min_dt_s) {
    throw std::invalid_argument("max_dt_s must be >= min_dt_s");
  }
  require_positive_finite(options_.max_speed_mps, "max_speed_mps");
  require_positive_finite(options_.max_acceleration_mps2,
                          "max_acceleration_mps2");
  require_positive_finite(options_.max_deceleration_mps2,
                          "max_deceleration_mps2");
  require_nonnegative_finite(options_.max_jerk_mps3, "max_jerk_mps3");
  require_positive_finite(options_.max_front_steering_angle_rad,
                          "max_front_steering_angle_rad");
  require_positive_finite(options_.max_rear_steering_angle_rad,
                          "max_rear_steering_angle_rad");
  require_positive_finite(options_.max_front_steering_rate_radps,
                          "max_front_steering_rate_radps");
  require_positive_finite(options_.max_rear_steering_rate_radps,
                          "max_rear_steering_rate_radps");
  require_positive_finite(options_.brake_deceleration_mps2,
                          "brake_deceleration_mps2");
  require_positive_finite(options_.emergency_deceleration_mps2,
                          "emergency_deceleration_mps2");
  require_positive_finite(options_.goal_tolerance_m, "goal_tolerance_m");
  require_positive_finite(options_.yaw_tolerance_rad, "yaw_tolerance_rad");
}

void KinematicVehiclePlant::reset(const PlantState &state) {
  if (!finite_state(state) || state.speed_mps < 0.0 || state.gear != 1) {
    throw std::invalid_argument(
        "reset state must be finite, nonnegative, and DRIVE");
  }
  state_ = state;
  state_.yaw_rad = normalize_angle(state_.yaw_rad);
  state_.speed_mps = std::clamp(state_.speed_mps, 0.0, options_.max_speed_mps);
  state_.front_steering_angle_rad = std::clamp(
      state_.front_steering_angle_rad, -options_.max_front_steering_angle_rad,
      options_.max_front_steering_angle_rad);
  state_.rear_steering_angle_rad = std::clamp(
      state_.rear_steering_angle_rad, -options_.max_rear_steering_angle_rad,
      options_.max_rear_steering_angle_rad);
}

PlantStepResult KinematicVehiclePlant::step(const PlantCommand &command,
                                            double dt_s) {
  if (!std::isfinite(dt_s) || dt_s < options_.min_dt_s ||
      dt_s > options_.max_dt_s) {
    force_stop("invalid_dt");
    return {false, true, "invalid_dt"};
  }
  if (!finite_command(command) || command.target_speed_mps < 0.0 ||
      command.brake < 0.0) {
    force_stop("invalid_command");
    return {false, true, "invalid_command"};
  }

  bool stop_requested = false;
  std::string stop_reason = "none";
  double stop_deceleration = options_.max_deceleration_mps2;
  if (command.command_timed_out) {
    stop_requested = true;
    stop_reason = "command_timeout";
    stop_deceleration = options_.brake_deceleration_mps2;
  } else if (command.emergency_stop) {
    stop_requested = true;
    stop_reason = "emergency_stop";
    stop_deceleration = options_.emergency_deceleration_mps2;
  } else if (!command.enable) {
    stop_requested = true;
    stop_reason = "disabled";
    stop_deceleration = options_.brake_deceleration_mps2;
  } else if (command.brake > 0.0) {
    stop_requested = true;
    stop_reason = "brake";
    stop_deceleration = options_.brake_deceleration_mps2;
  } else if (command.gear == 2) {
    stop_requested = true;
    stop_reason = "reverse_unsupported";
  } else if (command.gear != 1) {
    stop_requested = true;
    stop_reason = "unsupported_gear";
  }

  const double target_speed =
      stop_requested
          ? 0.0
          : std::clamp(command.target_speed_mps, 0.0, options_.max_speed_mps);
  const double target_front =
      stop_requested ? 0.0
                     : std::clamp(command.front_steering_angle_rad,
                                  -options_.max_front_steering_angle_rad,
                                  options_.max_front_steering_angle_rad);
  const double target_rear =
      stop_requested ? 0.0
                     : std::clamp(command.rear_steering_angle_rad,
                                  -options_.max_rear_steering_angle_rad,
                                  options_.max_rear_steering_angle_rad);

  state_.front_steering_angle_rad =
      move_toward(state_.front_steering_angle_rad, target_front,
                  options_.max_front_steering_rate_radps * dt_s);
  state_.rear_steering_angle_rad =
      move_toward(state_.rear_steering_angle_rad, target_rear,
                  options_.max_rear_steering_rate_radps * dt_s);

  const double old_speed = state_.speed_mps;
  double desired_acceleration = (target_speed - old_speed) / dt_s;
  if (target_speed >= old_speed) {
    desired_acceleration =
        std::clamp(desired_acceleration, 0.0, options_.max_acceleration_mps2);
    if (command.target_acceleration_mps2 > 1e-9 && !stop_requested) {
      desired_acceleration =
          std::min(desired_acceleration,
                   std::clamp(command.target_acceleration_mps2, 0.0,
                              options_.max_acceleration_mps2));
    }
  } else {
    const double deceleration =
        stop_requested ? stop_deceleration : options_.max_deceleration_mps2;
    desired_acceleration = std::clamp(desired_acceleration, -deceleration, 0.0);
  }
  if (options_.max_jerk_mps3 > 0.0) {
    desired_acceleration =
        move_toward(state_.acceleration_mps2, desired_acceleration,
                    options_.max_jerk_mps3 * dt_s);
  }

  double new_speed = std::clamp(old_speed + desired_acceleration * dt_s, 0.0,
                                options_.max_speed_mps);
  if ((target_speed >= old_speed && new_speed > target_speed) ||
      (target_speed < old_speed && new_speed < target_speed)) {
    new_speed = target_speed;
  }
  const double actual_acceleration = (new_speed - old_speed) / dt_s;
  const double midpoint_speed = 0.5 * (old_speed + new_speed);
  const double curvature = (std::tan(state_.front_steering_angle_rad) -
                            std::tan(state_.rear_steering_angle_rad)) /
                           options_.wheel_base_m;
  const double yaw_rate = midpoint_speed * curvature;
  const double midpoint_yaw = state_.yaw_rad + 0.5 * yaw_rate * dt_s;
  state_.x_m += midpoint_speed * std::cos(midpoint_yaw) * dt_s;
  state_.y_m += midpoint_speed * std::sin(midpoint_yaw) * dt_s;
  state_.yaw_rad = normalize_angle(state_.yaw_rad + yaw_rate * dt_s);
  state_.speed_mps = new_speed;
  state_.acceleration_mps2 = actual_acceleration;
  state_.gear = 1;
  if (stop_requested && state_.speed_mps <= 1e-9) {
    state_.speed_mps = 0.0;
    state_.acceleration_mps2 = 0.0;
  }

  if (!finite_state(state_)) {
    force_stop("non_finite_state");
    return {false, true, "non_finite_state"};
  }
  return {true, stop_requested, stop_reason};
}

const PlantState &KinematicVehiclePlant::state() const { return state_; }

bool KinematicVehiclePlant::goal_reached(double goal_x_m, double goal_y_m,
                                         double goal_yaw_rad) const {
  if (!std::isfinite(goal_x_m) || !std::isfinite(goal_y_m) ||
      !std::isfinite(goal_yaw_rad)) {
    return false;
  }
  return std::hypot(state_.x_m - goal_x_m, state_.y_m - goal_y_m) <=
             options_.goal_tolerance_m &&
         std::abs(normalize_angle(state_.yaw_rad - goal_yaw_rad)) <=
             options_.yaw_tolerance_rad;
}

void KinematicVehiclePlant::force_stop(const std::string &) {
  state_.speed_mps = 0.0;
  state_.acceleration_mps2 = 0.0;
  state_.front_steering_angle_rad = 0.0;
  state_.rear_steering_angle_rad = 0.0;
  state_.gear = 1;
}

} // namespace low_speed_av_simulation
