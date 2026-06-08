#include "low_speed_av_planning/astar_planner.hpp"

#include <algorithm>
#include <map>
#include <queue>

namespace low_speed_av_planning {

PlanResult AstarPlanner::plan(
  const TopologyGraph & graph,
  const std::string & start_node_id,
  const std::string & goal_node_id,
  const GlobalPlannerOptions & options) const
{
  struct Item { double f; double g; std::string node; };
  struct Greater { bool operator()(const Item & a, const Item & b) const { return a.f > b.f; } };
  std::priority_queue<Item, std::vector<Item>, Greater> open;
  std::map<std::string, double> best_g;
  std::map<std::string, std::string> parent_node;
  std::map<std::string, std::string> parent_edge;
  open.push({0.0, 0.0, start_node_id});
  best_g[start_node_id] = 0.0;
  while (!open.empty()) {
    // A* differs from Dijkstra only by adding geometric heuristic cost.
    const auto current = open.top();
    open.pop();
    if (current.node == goal_node_id) {
      break;
    }
    if (current.g > best_g[current.node]) {
      continue;
    }
    for (const auto & edge : graph.outgoing(current.node)) {
      if (!edge.enabled || edge.blocked_by_default || options.blocked_edges.count(edge.id) > 0) {
        continue;
      }
      if (!options.allow_reverse && edge.direction == "reverse") {
        continue;
      }
      const double g = current.g + edge.cost;
      if (!best_g.count(edge.to_node_id) || g < best_g[edge.to_node_id]) {
        best_g[edge.to_node_id] = g;
        parent_node[edge.to_node_id] = current.node;
        parent_edge[edge.to_node_id] = edge.id;
        const double h = options.heuristic_weight * graph.heuristic_distance(edge.to_node_id, goal_node_id);
        open.push({g + h, g, edge.to_node_id});
      }
    }
  }
  PlanResult result;
  result.planner_algorithm = name();
  if (!best_g.count(goal_node_id)) {
    result.message = "no valid global route";
    return result;
  }
  for (std::string node = goal_node_id; !node.empty();) {
    result.node_ids.push_back(node);
    if (node == start_node_id) {
      break;
    }
    result.edge_ids.push_back(parent_edge[node]);
    node = parent_node[node];
  }
  std::reverse(result.node_ids.begin(), result.node_ids.end());
  std::reverse(result.edge_ids.begin(), result.edge_ids.end());
  for (const auto & edge_id : result.edge_ids) {
    if (const auto * edge = graph.edge(edge_id)) {
      result.length_m += edge->length_m;
      result.estimated_time_s += edge->length_m / std::max(edge->speed_limit_mps, 0.1);
    }
  }
  result.success = true;
  result.message = "ok";
  return result;
}

}  // namespace low_speed_av_planning
