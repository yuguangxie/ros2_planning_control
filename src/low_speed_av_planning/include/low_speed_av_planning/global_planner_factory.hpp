#pragma once

#include <memory>
#include <string>

#include "low_speed_av_planning/global_planner_base.hpp"

namespace low_speed_av_planning {

class GlobalPlannerFactory {
public:
  static std::unique_ptr<GlobalPlannerBase> create(const std::string & algorithm);
};

}  // namespace low_speed_av_planning
