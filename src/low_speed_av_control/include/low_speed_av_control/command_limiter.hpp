#pragma once

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

class CommandLimiter {
public:
  // Clamp command values and convert non-finite commands into emergency stop.
  ControlCommand limit(const ControlCommand & command, const VehicleLimits & limits) const;
};

}  // namespace low_speed_av_control
