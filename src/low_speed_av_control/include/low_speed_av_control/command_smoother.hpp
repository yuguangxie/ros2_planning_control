#pragma once

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

struct SmootherOptions {
  double max_accel_mps2{0.5};
  double max_decel_mps2{0.8};
  double max_jerk_mps3{2.0};
  double max_front_steer_rate_radps{0.5};
  double max_rear_steer_rate_radps{0.5};
  double min_dt_s{0.001};
  double max_dt_s{0.1};
};

struct SmootherDiagnostics {
  double applied_dt_s{0.0};
  double applied_acceleration_mps2{0.0};
  bool dt_clamped{false};
  bool speed_limited{false};
  bool jerk_limited{false};
  bool front_steer_rate_limited{false};
  bool rear_steer_rate_limited{false};
  bool safety_bypass{false};
};

class CommandSmoother {
public:
  // Smooth target command using the previous output command as state.
  ControlCommand smooth(const ControlCommand &target,
                        const SmootherOptions &options, double actual_dt_s);
  void reset();
  const SmootherDiagnostics &diagnostics() const { return diagnostics_; }

private:
  ControlCommand previous_;
  double previous_acceleration_mps2_{0.0};
  bool initialized_{false};
  SmootherDiagnostics diagnostics_;
};

} // namespace low_speed_av_control
