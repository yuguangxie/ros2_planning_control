#include "low_speed_av_planning/dijkstra_planner.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <tuple>

namespace low_speed_av_planning {
namespace {

bool edge_allowed(const TopologyEdge & edge, const GlobalPlannerOptions & options)
{
  // Apply runtime closure and direction rules before adding an edge to search.
  if (!edge.enabled || edge.blocked_by_default || options.blocked_edges.count(edge.id) > 0) {
    return false;
  }
  return options.allow_reverse || edge.direction != "reverse";
}

}  // namespace

PlanResult DijkstraPlanner::plan(
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
  for (const auto & item : graph.edges()) {
    if (!std::isfinite(item.second.cost) || item.second.cost < 0.0) {
      result.message = "graph contains invalid negative/non-finite edge cost: " + item.first;
      return result;
    }
  }
  struct Item { double cost; std::string node; };
  struct Greater {
    bool operator()(const Item & a, const Item & b) const
    {
      return std::tie(a.cost, a.node) > std::tie(b.cost, b.node);
    }
  };

  std::priority_queue<Item, std::vector<Item>, Greater> open;
  std::map<std::string, double> best;
  std::map<std::string, std::string> parent_node;
  std::map<std::string, std::string> parent_edge;
  open.push({0.0, start_node_id});
  best[start_node_id] = 0.0;

  while (!open.empty()) {
    // Standard Dijkstra expansion over directed topology edges.
    const auto current = open.top();
    open.pop();
    if (current.node == goal_node_id) {
      break;
    }
    if (current.cost > best[current.node]) {
      continue;
    }
    for (const auto & edge : graph.outgoing(current.node)) {
      if (!edge_allowed(edge, options)) {
        continue;
      }
      const double next_cost = current.cost + edge.cost;
      const bool better = !best.count(edge.to_node_id) || next_cost < best[edge.to_node_id] - 1e-12;
      const bool equal_but_stable = best.count(edge.to_node_id) &&
        std::fabs(next_cost - best[edge.to_node_id]) <= 1e-12 &&
        std::tie(current.node, edge.id) <
        std::tie(parent_node[edge.to_node_id], parent_edge[edge.to_node_id]);
      if (better || equal_but_stable) {
        best[edge.to_node_id] = next_cost;
        parent_node[edge.to_node_id] = current.node;
        parent_edge[edge.to_node_id] = edge.id;
        open.push({next_cost, edge.to_node_id});
      }
    }
  }

  if (!best.count(goal_node_id)) {
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
