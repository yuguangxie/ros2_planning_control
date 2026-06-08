#pragma once

#include "low_speed_av_control/stanley_controller.hpp"

namespace low_speed_av_control {

class LqrController final : public ControllerBase {
public:
  std::string name() const override { return "lqr"; }
  ControlCommand compute(
    const Pose2d & pose,
    const VehicleState & state,
    const Trajectory & trajectory,
    const ControllerOptions & options) const override;
};

}  // namespace low_speed_av_control
