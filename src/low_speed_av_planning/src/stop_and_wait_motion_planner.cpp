#include "low_speed_av_planning/stop_and_wait_motion_planner.hpp"

#include "low_speed_av_planning/reference_line_motion_planner.hpp"

namespace low_speed_av_planning {

Trajectory StopAndWaitMotionPlanner::make_trajectory(
  const RoadnetPackage & package,
  const std::vector<std::string> & edge_ids,
  const Pose2d * crop_pose,
  const MotionPlannerOptions & options) const
{
  ReferenceLineMotionPlanner reference;
  auto trajectory = reference.make_trajectory(package, edge_ids, crop_pose, options);
  if (trajectory.empty() && !package.waypoints.empty()) {
    trajectory.push_back(package.waypoints.front());
  }
  for (auto & wp : trajectory) {
    wp.target_speed_mps = 0.0;
    wp.behavior = "stop_and_wait";
  }
  return trajectory;
}

}  // namespace low_speed_av_planning
