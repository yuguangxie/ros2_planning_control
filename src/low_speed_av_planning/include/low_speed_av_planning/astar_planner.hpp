#pragma once

#include "low_speed_av_planning/global_planner_base.hpp"

namespace low_speed_av_planning {

class AstarPlanner final : public GlobalPlannerBase {
public:
  std::string name() const override { return "astar"; }
  PlanResult plan(
    const TopologyGraph & graph,
    const std::string & start_node_id,
    const std::string & goal_node_id,
    const GlobalPlannerOptions & options) const override;
};

}  // namespace low_speed_av_planning
