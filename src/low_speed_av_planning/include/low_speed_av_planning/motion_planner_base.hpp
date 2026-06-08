#pragma once

#include <memory>
#include <string>

#include "low_speed_av_planning/roadnet_types.hpp"

namespace low_speed_av_planning {

struct MotionPlannerOptions {
  // Distance horizon for published reference trajectory.
  double horizon_distance_m{15.0};
  bool deduplicate_edge_boundary_points{true};
  bool regenerate_route_s{true};
};

class MotionPlannerBase {
public:
  virtual ~MotionPlannerBase() = default;
  virtual std::string name() const = 0;
  // Convert a global edge sequence into trajectory points. crop_pose is optional
  // and lets a runtime node publish only the useful forward horizon.
  virtual Trajectory make_trajectory(
    const RoadnetPackage & package,
    const std::vector<std::string> & edge_ids,
    const Pose2d * crop_pose,
    const MotionPlannerOptions & options) const = 0;
};

class MotionPlannerFactory {
public:
  static std::unique_ptr<MotionPlannerBase> create(const std::string & algorithm);
};

}  // namespace low_speed_av_planning
