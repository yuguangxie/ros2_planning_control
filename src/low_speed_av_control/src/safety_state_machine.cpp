#include "low_speed_av_control/safety_state_machine.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_control {

void SafetyEstopLatch::update(bool estop_requested, bool latch_enabled)
{
  if (estop_requested) {
    latched_ = true;
  } else if (!latch_enabled) {
    latched_ = false;
  }
}

void SafetyEstopLatch::clear_explicit()
{
  latched_ = false;
}

SafetyDecision ControlSafetyStateMachine::evaluate(const SafetyInputs & inputs) const
{
  if (inputs.safety_request_active || inputs.safety_estop_latched) {
    return {ControlSafetyState::EstopLatched, false, true, 2, "safety_estop_latched"};
  }
  if (inputs.trajectory_emergency) {
    return {ControlSafetyState::EstopLatched, false, true, 2, "trajectory_emergency_stop"};
  }

  if (inputs.have_vehicle_state) {
    if (!inputs.vehicle_state_valid) {
      return {ControlSafetyState::ControlledStop, false, false, 2, "invalid_vehicle_state"};
    }
    if (inputs.vehicle_state_timed_out) {
      return {ControlSafetyState::ControlledStop, false, false, 1, "vehicle_state_timeout"};
    }
    if (!inputs.fault_code.empty()) {
      return {ControlSafetyState::ControlledStop, false, true, 2, "vehicle_fault:" + inputs.fault_code};
    }
    if (!inputs.autonomous_enabled) {
      return {ControlSafetyState::ControlledStop, false, false, 1, "autonomous_disabled"};
    }
    if (inputs.brake_pressed) {
      return {ControlSafetyState::ControlledStop, false, false, 1, "vehicle_brake_pressed"};
    }
  } else if (inputs.vehicle_state_required) {
    return {ControlSafetyState::WaitInputs, false, false, 1, "waiting_for_vehicle_state"};
  }

  if (!inputs.have_pose) {
    return {ControlSafetyState::WaitInputs, false, false, 1, "waiting_for_localization"};
  }
  if (!inputs.pose_valid) {
    return {ControlSafetyState::ControlledStop, false, false, 2, "invalid_localization"};
  }
  if (inputs.pose_timed_out) {
    return {ControlSafetyState::ControlledStop, false, false, 1, "localization_timeout"};
  }

  if (!inputs.have_trajectory) {
    return {ControlSafetyState::WaitInputs, false, false, 1, "waiting_for_trajectory"};
  }
  if (!inputs.trajectory_valid) {
    const auto reason = inputs.trajectory_invalid_reason.empty() ?
      "invalid_trajectory" : inputs.trajectory_invalid_reason;
    return {ControlSafetyState::ControlledStop, false, false, 2, reason};
  }
  if (inputs.trajectory_timed_out) {
    return {ControlSafetyState::ControlledStop, false, false, 1, "trajectory_timeout"};
  }

  if (inputs.force_ready_cycle) {
    return {ControlSafetyState::Ready, false, false, 0, "inputs_ready_after_estop_clear"};
  }
  return {ControlSafetyState::Active, true, false, 0, "tracking_trajectory"};
}

std::string ControlSafetyStateMachine::state_name(ControlSafetyState state)
{
  switch (state) {
    case ControlSafetyState::WaitInputs:
      return "WAIT_INPUTS";
    case ControlSafetyState::Ready:
      return "READY";
    case ControlSafetyState::Active:
      return "ACTIVE";
    case ControlSafetyState::ControlledStop:
      return "CONTROLLED_STOP";
    case ControlSafetyState::EstopLatched:
      return "ESTOP_LATCHED";
  }
  return "CONTROLLED_STOP";
}

bool validate_trajectory_input(
  const Trajectory & trajectory,
  const std::string & trajectory_id,
  const std::string & source_package_id,
  const std::string & status,
  bool emergency_stop,
  const std::vector<std::string> & allowed_statuses,
  double s_tolerance_m,
  std::string * reason)
{
  auto fail = [reason](const std::string & text) {
      if (reason) {
        *reason = text;
      }
      return false;
    };

  if (emergency_stop) {
    return fail("trajectory_emergency_stop");
  }
  if (trajectory_id.empty()) {
    return fail("trajectory_id_empty");
  }
  if (source_package_id.empty()) {
    return fail("trajectory_source_package_id_empty");
  }
  if (std::find(allowed_statuses.begin(), allowed_statuses.end(), status) == allowed_statuses.end()) {
    return fail("trajectory_status_rejected:" + (status.empty() ? std::string("empty") : status));
  }
  if (trajectory.empty()) {
    return fail("empty_trajectory");
  }

  const double tolerance = std::max(0.0, s_tolerance_m);
  for (std::size_t i = 0; i < trajectory.size(); ++i) {
    const auto & point = trajectory[i];
    if (!std::isfinite(point.x_m) || !std::isfinite(point.y_m) ||
      !std::isfinite(point.yaw_rad) || !std::isfinite(point.kappa_1pm) ||
      !std::isfinite(point.s_m) || !std::isfinite(point.v_mps))
    {
      return fail("trajectory_non_finite_point:" + std::to_string(i));
    }
    if (point.gear < 1 || point.gear > 3) {
      return fail("trajectory_invalid_gear:" + std::to_string(point.gear));
    }
    if (i > 0U && point.s_m + tolerance < trajectory[i - 1U].s_m) {
      return fail("trajectory_non_monotonic_s:" + std::to_string(i));
    }
  }
  if (reason) {
    reason->clear();
  }
  return true;
}

bool vehicle_state_is_finite(const VehicleState & state)
{
  return std::isfinite(state.speed_mps) &&
    std::isfinite(state.acceleration_mps2) &&
    std::isfinite(state.front_steering_angle_rad) &&
    std::isfinite(state.rear_steering_angle_rad);
}

EstopClearDecision evaluate_estop_clear(const EstopClearInputs & inputs)
{
  if (inputs.safety_request_active) {
    return {false, "latest safety status still requests estop"};
  }
  if (inputs.trajectory_emergency) {
    return {false, "latest trajectory still requests emergency stop"};
  }
  if (!inputs.localization_ready) {
    return {false, "localization is missing, invalid, or stale"};
  }
  if (!inputs.trajectory_ready) {
    return {false, "trajectory is missing, invalid, or stale"};
  }
  if (!inputs.have_vehicle_state) {
    return {false, inputs.vehicle_state_required ?
      "required vehicle state is missing" : "vehicle state is required for estop clear"};
  }
  if (inputs.have_vehicle_state) {
    if (!inputs.vehicle_state_ready) {
      return {false, "vehicle state is invalid or stale"};
    }
    if (!std::isfinite(inputs.vehicle_speed_mps) ||
      std::abs(inputs.vehicle_speed_mps) > inputs.max_clear_speed_mps)
    {
      return {false, "vehicle speed exceeds estop clear threshold"};
    }
    if (!inputs.fault_code.empty()) {
      return {false, "vehicle fault is active: " + inputs.fault_code};
    }
    if (inputs.brake_pressed) {
      return {false, "vehicle brake is pressed"};
    }
    if (!inputs.autonomous_enabled) {
      return {false, "autonomous mode is not enabled"};
    }
  }
  return {true, "ready_to_clear"};
}

}  // namespace low_speed_av_control
