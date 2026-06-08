#pragma once

#include "low_speed_av_planning/curvature_speed_planner.hpp"

namespace low_speed_av_planning {

class ObstacleAwareSpeedPlanner final : public SpeedPlannerBase {
public:
  std::string name() const override { return "obstacle_aware"; }
  void apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const override;
};

}  // namespace low_speed_av_planning
