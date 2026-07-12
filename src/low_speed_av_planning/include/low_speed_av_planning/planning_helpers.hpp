#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>

#include "low_speed_av_planning/roadnet_types.hpp"

namespace low_speed_av_planning {

struct RoadnetAnchor {
  enum class Type {CurrentPose, Node, EdgePoint, TaskPoint, ParkingPoint, ChargingPoint};
  Type type{Type::Node};
  std::string point_id;
  std::string node_id;
  std::string edge_id;
  std::string edge_from_node_id;
  std::string edge_to_node_id;
  double x_m{0.0};
  double y_m{0.0};
  double yaw_rad{0.0};
  double s_on_edge_m{0.0};
  double edge_progress{0.0};
  std::size_t waypoint_index{0};
  bool has_pose{false};
  bool has_edge{false};
  bool has_node{false};
  bool require_final_stop{false};
};

std::string normalize_optional_identifier(const std::string & value);
std::optional<RoadnetAnchor> make_node_anchor(
  const RoadnetPackage & package, const std::string & node_id, RoadnetAnchor::Type type,
  const std::string & point_id, std::string * diagnostic);
bool project_anchor_to_edge(
  const RoadnetPackage & package, RoadnetAnchor & anchor, std::string * diagnostic);
std::optional<RoadnetAnchor> make_semantic_anchor(
  const RoadnetPackage & package, const std::string & point_id,
  const std::map<std::string, SemanticPoint> & points, const std::string & point_kind,
  RoadnetAnchor::Type type, bool prefer_edge_to_node, std::string * diagnostic);
std::optional<RoadnetAnchor> make_current_pose_anchor(
  const RoadnetPackage & package, const Pose2d & pose, const std::string & matched_node_id,
  double max_heading_error_rad, double max_projection_distance_m,
  bool include_current_edge_prefix, std::string * diagnostic);

bool anchors_on_same_edge(const RoadnetAnchor & start, const RoadnetAnchor & goal);
bool goal_behind_on_same_edge(const RoadnetAnchor & start, const RoadnetAnchor & goal);
void regenerate_route_s(Trajectory & trajectory);
bool trajectory_is_continuous(
  const Trajectory & trajectory, double max_jump_m, std::string * diagnostic);
void apply_semantic_speed_limits(Trajectory & trajectory, const std::vector<SemanticArea> & areas);
void recompute_route_summary(PlanResult & route, const Trajectory & full_reference);
Trajectory build_edge_segment_between(
  const RoadnetPackage & package, const RoadnetAnchor & start, const RoadnetAnchor & goal,
  bool reverse);

struct ProgressSearchOptions {
  double horizon_distance_m{15.0};
  std::size_t backward_window_points{2U};
  std::size_t forward_window_points{200U};
  double max_heading_error_rad{1.5707963267948966};
};

class TrajectoryProgressTracker {
public:
  void reset();
  Trajectory crop(
    const Trajectory & full_reference, const std::optional<Pose2d> & pose,
    const std::string & trajectory_identity, const ProgressSearchOptions & options);
  std::size_t progress_index() const {return progress_index_;}

private:
  std::string trajectory_identity_;
  std::size_t progress_index_{0U};
  bool initialized_{false};
};

}  // namespace low_speed_av_planning
