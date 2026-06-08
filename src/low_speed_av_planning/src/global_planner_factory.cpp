#include "low_speed_av_planning/global_planner_factory.hpp"

#include <stdexcept>

#include "low_speed_av_planning/astar_planner.hpp"
#include "low_speed_av_planning/dijkstra_planner.hpp"

namespace low_speed_av_planning {

std::unique_ptr<GlobalPlannerBase> GlobalPlannerFactory::create(const std::string & algorithm)
{
  if (algorithm == "dijkstra") {
    return std::make_unique<DijkstraPlanner>();
  }
  if (algorithm == "astar") {
    return std::make_unique<AstarPlanner>();
  }
  throw std::invalid_argument("unknown global planner: " + algorithm);
}

}  // namespace low_speed_av_planning
