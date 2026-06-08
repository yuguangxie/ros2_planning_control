#pragma once

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

struct SmootherOptions {
  // Per-cycle speed step and steering-rate settings.
  double max_speed_step_mps{0.05};
  double max_steer_rate_radps{0.35};
  double dt_s{0.02};
};

class CommandSmoother {
public:
  // Smooth target command using the previous output command as state.
  ControlCommand smooth(const ControlCommand & target, const SmootherOptions & options);

private:
  ControlCommand previous_;
};

}  // namespace low_speed_av_control
