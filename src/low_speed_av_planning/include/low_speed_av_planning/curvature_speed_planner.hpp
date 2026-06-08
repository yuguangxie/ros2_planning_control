#pragma once

#include "low_speed_av_planning/speed_planner_base.hpp"

namespace low_speed_av_planning {

class CurvatureSpeedPlanner final : public SpeedPlannerBase {
public:
  std::string name() const override { return "curvature"; }
  void apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const override;
};

}  // namespace low_speed_av_planning
