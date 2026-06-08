#pragma once

#include "low_speed_av_control/vehicle_model_base.hpp"

namespace low_speed_av_control {

class FrontAckermannModel final : public VehicleModelBase {
public:
  std::string name() const override { return "front_ackermann"; }
  ControlCommand steering_from_curvature(double kappa_1pm, const VehicleLimits & limits) const override;
};

}  // namespace low_speed_av_control
