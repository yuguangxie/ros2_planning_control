#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "low_speed_av_control/command_smoother.hpp"
#include "low_speed_av_control/control_types.hpp"
#include "low_speed_av_control/controller_base.hpp"

namespace low_speed_av_control {

struct ControlTimingOptions {
  double control_rate_hz{50.0};
  double localization_timeout_s{0.2};
  double trajectory_timeout_s{0.5};
  double vehicle_state_timeout_s{0.5};
  double status_publish_rate_hz{5.0};
  double publish_deadline_warning_s{0.1};
  double hardware_watchdog_timeout_s{0.5};
  std::size_t cadence_window_size{128U};
  std::string hardware_watchdog_contract_status{"DECLARED_NOT_HIL_VERIFIED"};
};

struct TrackingProgressOptions {
  std::size_t backward_window_points{3U};
  std::size_t forward_window_points{200U};
  double max_heading_error_rad{1.57};
};

struct ControlConfiguration {
  std::string output_mode{"both"};
  std::string controller_algorithm{"lqr"};
  std::string vehicle_model{"front_ackermann"};
  std::vector<std::string> allowed_trajectory_statuses{"ok"};
  double trajectory_s_tolerance_m{1.0e-4};
  double clear_speed_threshold_mps{0.05};
  VehicleLimits vehicle_limits;
  ControllerOptions controller_options;
  SmootherOptions smoother_options;
  ScuCommandOptions scu_options;
  ControlTimingOptions timing;
  TrackingProgressOptions progress;
};

void validate_control_configuration(const ControlConfiguration &configuration);

struct ControllerInputDecision {
  bool valid{false};
  std::string reason;
};

ControllerInputDecision
validate_controller_input(const Pose2d &pose, const VehicleState &state,
                          const Trajectory &trajectory,
                          const ControllerOptions &options);
ControlCommand controller_stop_command(const std::string &controller,
                                       const std::string &reason);

class TrackingProgressTracker {
public:
  void reset();
  Trajectory select_window(const Trajectory &trajectory, const Pose2d &pose,
                           int vehicle_gear,
                           const std::string &trajectory_identity,
                           const TrackingProgressOptions &options);
  std::size_t progress_index() const { return progress_index_; }

private:
  std::string trajectory_identity_;
  std::size_t progress_index_{0U};
  bool initialized_{false};
};

struct CadenceSnapshot {
  double last_interval_s{0.0};
  double max_interval_s{0.0};
  double p95_interval_s{0.0};
  double last_publish_age_s{0.0};
  uint64_t cycle_count{0U};
  uint64_t publish_count{0U};
  uint64_t missed_cycles{0U};
  uint64_t deadline_misses{0U};
  bool hardware_timeout_gap{false};
};

class ControlCadenceMonitor {
public:
  explicit ControlCadenceMonitor(
      const ControlTimingOptions &options = ControlTimingOptions{});
  void reset(const ControlTimingOptions &options);
  double observe_cycle(double steady_now_s);
  void mark_publish(double steady_now_s);
  CadenceSnapshot snapshot(double steady_now_s) const;

private:
  ControlTimingOptions options_;
  std::vector<double> intervals_;
  double last_cycle_time_s_{0.0};
  double last_publish_time_s_{0.0};
  double last_interval_s_{0.0};
  double max_interval_s_{0.0};
  uint64_t cycle_count_{0U};
  uint64_t publish_count_{0U};
  uint64_t missed_cycles_{0U};
  uint64_t deadline_misses_{0U};
  bool initialized_{false};
};

} // namespace low_speed_av_control
