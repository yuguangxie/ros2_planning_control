#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace low_speed_av_planning {

// Minimal 2D pose used by pure planning logic. Keeping this type independent
// from ROS messages makes algorithm tests possible without a ROS2 runtime.
struct Pose2d {
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
};

// Internal waypoint representation after mapping AD Package fields:
// x/y/yaw/kappa/v_mps/s_m become SI-unit planning fields.
struct Waypoint {
  std::size_t index{0};
  std::string waypoint_id;
  std::string edge_id;
  std::string path_id;
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
  double kappa_1pm{0.0};
  double edge_s_m{0.0};
  double route_s_m{0.0};
  double target_speed_mps{0.0};
  int gear{1};
  std::string behavior{"follow"};
  bool edge_start{false};
  bool edge_end{false};
};

// Waypoint ranges are half-open internally. Legacy inclusive end_index is
// converted to end_index_exclusive by RoadnetLoader.
struct WaypointRange {
  std::size_t start_index{0};
  std::size_t end_index_exclusive{0};
  std::size_t count{0};
  bool used_legacy_inclusive_end{false};
};

// Topology nodes and edges are the graph primitives consumed by Dijkstra/A*.
struct TopologyNode {
  std::string id;
  Pose2d pose;
  std::string type;
};

struct TopologyEdge {
  std::string id;
  std::string from_node_id;
  std::string to_node_id;
  std::string direction{"forward"};
  double length_m{0.0};
  double cost{1.0};
  double speed_limit_mps{0.5};
  bool enabled{true};
  bool blocked_by_default{false};
  bool allow_reverse{false};
};

struct SemanticPoint {
  std::string id;
  std::string type;
  Pose2d pose;
  std::string linked_node_id;
  std::string linked_edge_id;
  std::string linked_path_id;
  double linked_s_m{0.0};
};

struct SemanticArea {
  std::string id;
  std::string type;
  std::vector<Pose2d> polygon;
  double speed_limit_mps{0.0};
  bool allow_planning_through{true};
  int priority{0};
};

// Loaded AD Package cache. Planning code reads from this structure instead of
// repeatedly touching ZIP/export files.
struct RoadnetPackage {
  std::string root_path;
  std::string package_id;
  std::string schema_version;
  std::string global_frame{"map"};
  std::string control_reference_frame{"rear_axle"};
  std::map<std::string, std::string> units;
  std::map<std::string, std::string> files;
  std::map<std::string, std::string> manifest_hashes;
  std::vector<TopologyNode> nodes;
  std::vector<TopologyEdge> edges;
  std::vector<Waypoint> waypoints;
  std::map<std::string, WaypointRange> waypoint_index_by_edge;
  std::vector<SemanticArea> areas;
  std::map<std::string, SemanticPoint> route_points;
  std::map<std::string, SemanticPoint> task_points;
  std::map<std::string, SemanticPoint> parking_points;
  std::map<std::string, SemanticPoint> charging_points;
  std::set<std::string> blocked_edges;
  std::vector<std::string> warnings;
};

// Result shared by all global planners.
struct PlanResult {
  bool success{false};
  std::string message;
  std::string planner_algorithm;
  std::vector<std::string> node_ids;
  std::vector<std::string> edge_ids;
  double length_m{0.0};
  double estimated_time_s{0.0};
};

using Trajectory = std::vector<Waypoint>;

}  // namespace low_speed_av_planning
