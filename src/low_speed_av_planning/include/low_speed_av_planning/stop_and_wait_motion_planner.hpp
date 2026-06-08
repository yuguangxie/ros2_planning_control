#pragma once

#include "low_speed_av_planning/motion_planner_base.hpp"

namespace low_speed_av_planning {

class StopAndWaitMotionPlanner final : public MotionPlannerBase {
public:
  std::string name() const override { return "stop_and_wait"; }
  Trajectory make_trajectory(
    const RoadnetPackage & package,
    const std::vector<std::string> & edge_ids,
    const Pose2d * crop_pose,
    const MotionPlannerOptions & options) const override;
};

}  // namespace low_speed_av_planning
