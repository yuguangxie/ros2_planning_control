#pragma once

#include "low_speed_av_planning/reference_line_motion_planner.hpp"

namespace low_speed_av_planning {

class HybridAstarParkingPlanner final : public ReferenceLineMotionPlanner {
public:
  std::string name() const override { return "hybrid_astar_parking"; }
};

}  // namespace low_speed_av_planning
