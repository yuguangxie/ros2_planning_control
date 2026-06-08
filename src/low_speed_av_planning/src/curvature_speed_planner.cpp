#include "low_speed_av_planning/curvature_speed_planner.hpp"

#include <algorithm>
#include <cmath>

namespace low_speed_av_planning {

void CurvatureSpeedPlanner::apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const
{
  for (auto & wp : trajectory) {
    // Limit speed by lateral acceleration: v <= sqrt(a_lat / |kappa|).
    const double abs_k = std::abs(wp.kappa_1pm);
    const double curvature_limit = abs_k > 1e-6 ?
      std::sqrt(std::max(options.max_lateral_accel_mps2, 0.01) / abs_k) :
      options.max_speed_mps;
    const double editor_speed = wp.target_speed_mps > 0.0 ? wp.target_speed_mps : options.default_speed_mps;
    wp.target_speed_mps = std::clamp(std::min(editor_speed, curvature_limit), 0.0, options.max_speed_mps);
    if (!std::isfinite(wp.target_speed_mps)) {
      wp.target_speed_mps = 0.0;
    }
  }
}

}  // namespace low_speed_av_planning
