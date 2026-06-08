#pragma once

#include <map>
#include <string>
#include <vector>

#include "low_speed_av_planning/roadnet_types.hpp"

namespace low_speed_av_planning {

class TopologyGraph {
public:
  explicit TopologyGraph(const RoadnetPackage & package);
  const TopologyNode * node(const std::string & id) const;
  const TopologyEdge * edge(const std::string & id) const;
  const std::vector<TopologyEdge> & outgoing(const std::string & node_id) const;
  double heuristic_distance(const std::string & from, const std::string & to) const;

private:
  std::map<std::string, TopologyNode> nodes_;
  std::map<std::string, TopologyEdge> edges_;
  std::map<std::string, std::vector<TopologyEdge>> adjacency_;
  std::vector<TopologyEdge> empty_;
};

}  // namespace low_speed_av_planning
