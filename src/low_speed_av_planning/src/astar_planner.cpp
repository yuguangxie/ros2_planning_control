#include "low_speed_av_planning/astar_planner.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <tuple>

namespace low_speed_av_planning {

PlanResult AstarPlanner::plan(
  const TopologyGraph & graph,
  const std::string & start_node_id,
  const std::string & goal_node_id,
  const GlobalPlannerOptions & options) const
{
  PlanResult result;
  result.planner_algorithm = name();
  if (!graph.node(start_node_id) || !graph.node(goal_node_id)) {
    result.message = "start or goal node is not in topology";
    return result;
  }
  if (!std::isfinite(options.heuristic_weight) || options.heuristic_weight < 0.0) {
    result.message = "heuristic_weight must be finite and non-negative";
    return result;
  }
  double minimum_cost_per_meter = std::numeric_limits<double>::infinity();
  for (const auto & item : graph.edges()) {
    const auto & edge = item.second;
    if (!std::isfinite(edge.cost) || edge.cost < 0.0) {
      result.message = "graph contains invalid negative/non-finite edge cost: " + item.first;
      return result;
    }
    const double distance = graph.heuristic_distance(edge.from_node_id, edge.to_node_id);
    if (distance > 1e-12) {
      minimum_cost_per_meter = std::min(minimum_cost_per_meter, edge.cost / distance);
    }
  }
  if (!std::isfinite(minimum_cost_per_meter)) {
    minimum_cost_per_meter = 0.0;
  }
  struct Item { double f; double g; std::string node; };
  struct Greater {
    bool operator()(const Item & a, const Item & b) const
    {
      return std::tie(a.f, a.g, a.node) > std::tie(b.f, b.g, b.node);
    }
  };
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
      const bool better = !best_g.count(edge.to_node_id) || g < best_g[edge.to_node_id] - 1e-12;
      const bool equal_but_stable = best_g.count(edge.to_node_id) &&
        std::fabs(g - best_g[edge.to_node_id]) <= 1e-12 &&
        std::tie(current.node, edge.id) <
        std::tie(parent_node[edge.to_node_id], parent_edge[edge.to_node_id]);
      if (better || equal_but_stable) {
        best_g[edge.to_node_id] = g;
        parent_node[edge.to_node_id] = current.node;
        parent_edge[edge.to_node_id] = edge.id;
        const double h = options.heuristic_weight * minimum_cost_per_meter *
          graph.heuristic_distance(edge.to_node_id, goal_node_id);
        open.push({g + h, g, edge.to_node_id});
      }
    }
  }
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
  result.message = options.heuristic_weight > 1.0 ? "ok_weighted_astar_non_optimal" : "ok";
  return result;
}

}  // namespace low_speed_av_planning
