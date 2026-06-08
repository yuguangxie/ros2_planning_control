#include "low_speed_av_planning/obstacle_aware_speed_planner.hpp"

#include <stdexcept>

#include "low_speed_av_planning/constant_speed_planner.hpp"

namespace low_speed_av_planning {

void ObstacleAwareSpeedPlanner::apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const
{
  CurvatureSpeedPlanner curvature;
  curvature.apply(trajectory, options);
  // Stub behavior: when a single obstacle distance is close, command all
  // downstream points to stop.
  if (options.obstacle_distance_m >= 0.0 && options.obstacle_distance_m <= options.obstacle_stop_distance_m) {
    for (auto & wp : trajectory) {
      if (wp.route_s_m >= options.obstacle_distance_m) {
        wp.target_speed_mps = 0.0;
        wp.behavior = "obstacle_stop";
      }
    }
  }
}

std::unique_ptr<SpeedPlannerBase> SpeedPlannerFactory::create(const std::string & algorithm)
{
  if (algorithm == "constant") {
    return std::make_unique<ConstantSpeedPlanner>();
  }
  if (algorithm == "curvature") {
    return std::make_unique<CurvatureSpeedPlanner>();
  }
  if (algorithm == "obstacle_aware") {
    return std::make_unique<ObstacleAwareSpeedPlanner>();
  }
  throw std::invalid_argument("unknown speed planner: " + algorithm);
}

}  // namespace low_speed_av_planning
