#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

enum class ControlSafetyState {
  WaitInputs,
  Ready,
  Active,
  ControlledStop,
  EstopLatched
};

struct SafetyInputs {
  bool safety_request_active{false};
  bool safety_estop_latched{false};
  bool trajectory_emergency{false};

  bool have_pose{false};
  bool pose_valid{false};
  bool pose_timed_out{false};

  bool have_trajectory{false};
  bool trajectory_valid{false};
  bool trajectory_timed_out{false};
  std::string trajectory_invalid_reason;

  bool vehicle_state_required{false};
  bool have_vehicle_state{false};
  bool vehicle_state_valid{true};
  bool vehicle_state_timed_out{false};
  bool autonomous_enabled{true};
  bool brake_pressed{false};
  std::string fault_code;

  bool force_ready_cycle{false};
};

struct SafetyDecision {
  ControlSafetyState state{ControlSafetyState::WaitInputs};
  bool allow_tracking{false};
  bool emergency_stop{false};
  uint8_t level{1};
  std::string reason{"waiting_for_inputs"};
};

class SafetyEstopLatch {
public:
  void update(bool estop_requested, bool latch_enabled);
  void clear_explicit();
  bool is_latched() const {return latched_;}

private:
  bool latched_{false};
};

struct EstopClearInputs {
  bool safety_request_active{false};
  bool trajectory_emergency{false};
  bool localization_ready{false};
  bool trajectory_ready{false};
  bool vehicle_state_required{false};
  bool have_vehicle_state{false};
  bool vehicle_state_ready{false};
  double vehicle_speed_mps{0.0};
  double max_clear_speed_mps{0.05};
  bool autonomous_enabled{true};
  bool brake_pressed{false};
  std::string fault_code;
};

struct EstopClearDecision {
  bool allowed{false};
  std::string reason;
};

class ControlSafetyStateMachine {
public:
  SafetyDecision evaluate(const SafetyInputs & inputs) const;
  static std::string state_name(ControlSafetyState state);
};

bool validate_trajectory_input(
  const Trajectory & trajectory,
  const std::string & trajectory_id,
  const std::string & source_package_id,
  const std::string & status,
  bool emergency_stop,
  const std::vector<std::string> & allowed_statuses,
  double s_tolerance_m,
  std::string * reason);

bool vehicle_state_is_finite(const VehicleState & state);
EstopClearDecision evaluate_estop_clear(const EstopClearInputs & inputs);

}  // namespace low_speed_av_control
