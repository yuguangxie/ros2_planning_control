#pragma once

#include "low_speed_av_control/controller_base.hpp"

namespace low_speed_av_control {

class PurePursuitController final : public ControllerBase {
public:
  std::string name() const override { return "pure_pursuit"; }
  ControlCommand compute(
    const Pose2d & pose,
    const VehicleState & state,
    const Trajectory & trajectory,
    const ControllerOptions & options) const override;
};

}  // namespace low_speed_av_control
