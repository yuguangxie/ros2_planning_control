#pragma once

#include "low_speed_av_planning/reference_line_motion_planner.hpp"

namespace low_speed_av_planning {

class FrenetLiteMotionPlanner final : public ReferenceLineMotionPlanner {
public:
  std::string name() const override { return "frenet_lite"; }
};

}  // namespace low_speed_av_planning
