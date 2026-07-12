#include "low_speed_av_planning/topology_graph.hpp"

#include <algorithm>
#include <cmath>
#include <tuple>

namespace low_speed_av_planning {

TopologyGraph::TopologyGraph(const RoadnetPackage & package)
{
  for (const auto & node : package.nodes) {
    nodes_[node.id] = node;
  }
  for (const auto & edge : package.edges) {
    edges_[edge.id] = edge;
    adjacency_[edge.from_node_id].push_back(edge);
  }
  for (auto & item : adjacency_) {
    std::sort(item.second.begin(), item.second.end(), [](const auto & a, const auto & b) {
      return std::tie(a.to_node_id, a.id) < std::tie(b.to_node_id, b.id);
    });
  }
}

const std::map<std::string, TopologyEdge> & TopologyGraph::edges() const
{
  return edges_;
}

const TopologyNode * TopologyGraph::node(const std::string & id) const
{
  const auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}

const TopologyEdge * TopologyGraph::edge(const std::string & id) const
{
  const auto it = edges_.find(id);
  return it == edges_.end() ? nullptr : &it->second;
}

const std::vector<TopologyEdge> & TopologyGraph::outgoing(const std::string & node_id) const
{
  const auto it = adjacency_.find(node_id);
  return it == adjacency_.end() ? empty_ : it->second;
}

double TopologyGraph::heuristic_distance(const std::string & from, const std::string & to) const
{
  const auto * a = node(from);
  const auto * b = node(to);
  if (!a || !b) {
    return 0.0;
  }
  return std::hypot(a->pose.x_m - b->pose.x_m, a->pose.y_m - b->pose.y_m);
}

}  // namespace low_speed_av_planning
