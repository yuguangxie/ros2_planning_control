#include "low_speed_av_planning/constant_speed_planner.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_planning {

void ConstantSpeedPlanner::apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const
{
  const double speed = std::clamp(options.default_speed_mps, 0.0, options.max_speed_mps);
  for (auto & wp : trajectory) {
    wp.target_speed_mps = std::isfinite(speed) ? speed : 0.0;
  }
}

}  // namespace low_speed_av_planning
