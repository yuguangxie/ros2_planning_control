#pragma once

#include <set>
#include <string>

#include "low_speed_av_planning/roadnet_types.hpp"
#include "low_speed_av_planning/topology_graph.hpp"

namespace low_speed_av_planning {

struct GlobalPlannerOptions {
  // Runtime block list, useful for temporarily closing an edge.
  bool allow_reverse{true};
  std::set<std::string> blocked_edges;
  double heuristic_weight{1.0};
};

class GlobalPlannerBase {
public:
  virtual ~GlobalPlannerBase() = default;
  virtual std::string name() const = 0;
  // Return edge/node sequence between graph nodes. Implementations must be
  // deterministic so they can be unit-tested without ROS2.
  virtual PlanResult plan(
    const TopologyGraph & graph,
    const std::string & start_node_id,
    const std::string & goal_node_id,
    const GlobalPlannerOptions & options) const = 0;
};

}  // namespace low_speed_av_planning
