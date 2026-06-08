#pragma once

#include <string>

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

class VehicleModelBase {
public:
  virtual ~VehicleModelBase() = default;
  virtual std::string name() const = 0;
  // Convert desired path curvature to front/rear steering commands.
  virtual ControlCommand steering_from_curvature(double kappa_1pm, const VehicleLimits & limits) const = 0;
};

}  // namespace low_speed_av_control
