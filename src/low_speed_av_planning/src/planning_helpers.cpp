#include "low_speed_av_planning/planning_helpers.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>

#include "low_speed_av_planning/topology_graph.hpp"

namespace low_speed_av_planning {
namespace {
double distance(const Waypoint & a, const Waypoint & b) {return std::hypot(a.x_m - b.x_m, a.y_m - b.y_m);}
double angle_difference(double a, double b) {return std::atan2(std::sin(a - b), std::cos(a - b));}
bool point_in_polygon(const Pose2d & point, const std::vector<Pose2d> & polygon)
{
  if (polygon.size() < 3U) {return false;}
  bool inside = false;
  for (std::size_t i = 0U, j = polygon.size() - 1U; i < polygon.size(); j = i++) {
    const auto & a = polygon[i];
    const auto & b = polygon[j];
    const bool crosses = ((a.y_m > point.y_m) != (b.y_m > point.y_m)) &&
      (point.x_m < (b.x_m - a.x_m) * (point.y_m - a.y_m) /
      ((b.y_m - a.y_m) == 0.0 ? 1e-12 : (b.y_m - a.y_m)) + a.x_m);
    inside = inside != crosses;
  }
  return inside;
}
void append_unique(Trajectory & trajectory, Waypoint waypoint)
{
  if (trajectory.empty() || distance(trajectory.back(), waypoint) >= 1e-4) {
    trajectory.push_back(std::move(waypoint));
  }
}
}  // namespace

std::string normalize_optional_identifier(const std::string & value)
{
  auto text = value;
  const auto non_space = [](unsigned char c) {return !std::isspace(c);};
  text.erase(text.begin(), std::find_if(text.begin(), text.end(), non_space));
  text.erase(std::find_if(text.rbegin(), text.rend(), non_space).base(), text.end());
  auto lower = text;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return (lower.empty() || lower == "null" || lower == "none") ? std::string{} : text;
}

std::optional<RoadnetAnchor> make_node_anchor(
  const RoadnetPackage & package, const std::string & node_id, RoadnetAnchor::Type type,
  const std::string & point_id, std::string * diagnostic)
{
  const auto id = normalize_optional_identifier(node_id);
  const TopologyGraph graph(package);
  const auto * node = graph.node(id);
  if (!node) {
    if (diagnostic) {*diagnostic = "node is not in topology: " + id;}
    return std::nullopt;
  }
  RoadnetAnchor anchor;
  anchor.type = type;
  anchor.point_id = normalize_optional_identifier(point_id).empty() ? id : point_id;
  anchor.node_id = id;
  anchor.x_m = node->pose.x_m;
  anchor.y_m = node->pose.y_m;
  anchor.yaw_rad = node->pose.yaw_rad;
  anchor.has_pose = true;
  anchor.has_node = true;
  return anchor;
}

bool project_anchor_to_edge(
  const RoadnetPackage & package, RoadnetAnchor & anchor, std::string * diagnostic)
{
  const TopologyGraph graph(package);
  const auto * edge = graph.edge(normalize_optional_identifier(anchor.edge_id));
  const auto range = package.waypoint_index_by_edge.find(anchor.edge_id);
  if (!edge || range == package.waypoint_index_by_edge.end() || range->second.count == 0U) {
    if (diagnostic) {*diagnostic = "invalid or waypoint-less linked edge: " + anchor.edge_id;}
    return false;
  }
  auto best_index = range->second.start_index;
  double best_distance = std::numeric_limits<double>::infinity();
  const auto end = std::min(range->second.end_index_exclusive, package.waypoints.size());
  for (std::size_t i = range->second.start_index; i < end; ++i) {
    const double d = std::hypot(anchor.x_m - package.waypoints[i].x_m, anchor.y_m - package.waypoints[i].y_m);
    if (d < best_distance) {best_distance = d; best_index = i;}
  }
  anchor.has_edge = true;
  anchor.edge_from_node_id = edge->from_node_id;
  anchor.edge_to_node_id = edge->to_node_id;
  anchor.waypoint_index = best_index;
  anchor.s_on_edge_m = package.waypoints[best_index].edge_s_m;
  anchor.edge_progress = range->second.count > 1U ?
    static_cast<double>(best_index - range->second.start_index) /
    static_cast<double>(range->second.count - 1U) : 0.0;
  return true;
}

std::optional<RoadnetAnchor> make_semantic_anchor(
  const RoadnetPackage & package, const std::string & point_id,
  const std::map<std::string, SemanticPoint> & points, const std::string & point_kind,
  RoadnetAnchor::Type type, bool prefer_edge_to_node, std::string * diagnostic)
{
  const auto id = normalize_optional_identifier(point_id);
  const auto it = points.find(id);
  if (it == points.end()) {
    if (diagnostic) {*diagnostic = point_kind + " not found: " + id;}
    return std::nullopt;
  }
  const TopologyGraph graph(package);
  const auto & point = it->second;
  RoadnetAnchor anchor;
  anchor.type = type;
  anchor.point_id = point.id;
  anchor.x_m = point.pose.x_m; anchor.y_m = point.pose.y_m; anchor.yaw_rad = point.pose.yaw_rad;
  anchor.has_pose = true;
  anchor.require_final_stop = true;
  const auto linked_node = normalize_optional_identifier(point.linked_node_id);
  if (!linked_node.empty() && graph.node(linked_node)) {
    anchor.node_id = linked_node;
    anchor.has_node = true;
  }
  const auto linked_edge = normalize_optional_identifier(point.linked_edge_id);
  if (!linked_edge.empty()) {
    anchor.edge_id = linked_edge;
    if (!project_anchor_to_edge(package, anchor, diagnostic)) {return std::nullopt;}
    if (!anchor.has_node) {
      anchor.node_id = prefer_edge_to_node ? anchor.edge_to_node_id : anchor.edge_from_node_id;
      anchor.has_node = !anchor.node_id.empty();
    }
  }
  if (!anchor.has_node && !anchor.has_edge) {
    if (diagnostic) {*diagnostic = point_kind + " " + id + " has no valid linked node or edge";}
    return std::nullopt;
  }
  if (diagnostic) {
    *diagnostic = point_kind + " " + id + " resolved to node=" + anchor.node_id +
      (anchor.has_edge ? " edge=" + anchor.edge_id : "");
  }
  return anchor;
}

std::optional<RoadnetAnchor> make_current_pose_anchor(
  const RoadnetPackage & package, const Pose2d & pose, const std::string & matched_node_id,
  double max_heading_error_rad, double max_projection_distance_m,
  bool include_current_edge_prefix, std::string * diagnostic)
{
  auto anchor = make_node_anchor(
    package, matched_node_id, RoadnetAnchor::Type::CurrentPose, "current_pose", diagnostic);
  if (!anchor) {return std::nullopt;}
  anchor->x_m = pose.x_m; anchor->y_m = pose.y_m; anchor->yaw_rad = pose.yaw_rad;
  const Waypoint * best = nullptr;
  double best_distance = std::numeric_limits<double>::infinity();
  for (const auto & waypoint : package.waypoints) {
    if (std::fabs(angle_difference(pose.yaw_rad, waypoint.yaw_rad)) > max_heading_error_rad) {continue;}
    const double candidate = std::hypot(pose.x_m - waypoint.x_m, pose.y_m - waypoint.y_m);
    if (candidate < best_distance) {best_distance = candidate; best = &waypoint;}
  }
  if (!best) {
    if (diagnostic) {*diagnostic = "current pose has no heading-compatible waypoint";}
    return std::nullopt;
  }
  if (max_projection_distance_m > 0.0 && best_distance > max_projection_distance_m) {
    if (diagnostic) {*diagnostic = "current pose projection exceeds configured distance";}
    return std::nullopt;
  }
  anchor->edge_id = best->edge_id;
  if (!project_anchor_to_edge(package, *anchor, diagnostic)) {return std::nullopt;}
  if (include_current_edge_prefix && !anchor->edge_to_node_id.empty()) {
    anchor->node_id = anchor->edge_to_node_id;
  }
  return anchor;
}

bool anchors_on_same_edge(const RoadnetAnchor & start, const RoadnetAnchor & goal)
{return start.has_edge && goal.has_edge && !start.edge_id.empty() && start.edge_id == goal.edge_id;}
bool goal_behind_on_same_edge(const RoadnetAnchor & start, const RoadnetAnchor & goal)
{return anchors_on_same_edge(start, goal) && goal.s_on_edge_m + 1e-3 < start.s_on_edge_m;}

void regenerate_route_s(Trajectory & trajectory)
{
  if (trajectory.empty()) {return;}
  trajectory.front().route_s_m = 0.0;
  for (std::size_t i = 1U; i < trajectory.size(); ++i) {
    trajectory[i].route_s_m = trajectory[i - 1U].route_s_m + distance(trajectory[i - 1U], trajectory[i]);
  }
}

bool trajectory_is_continuous(const Trajectory & trajectory, double max_jump_m, std::string * diagnostic)
{
  if (max_jump_m <= 0.0 || !std::isfinite(max_jump_m)) {return true;}
  for (std::size_t i = 1U; i < trajectory.size(); ++i) {
    const auto jump = distance(trajectory[i - 1U], trajectory[i]);
    if (!std::isfinite(jump) || jump > max_jump_m) {
      if (diagnostic) {
        std::ostringstream out;
        out << "trajectory point jump too large: index=" << i << " distance_m=" << jump <<
          " max=" << max_jump_m << " from_edge=" << trajectory[i - 1U].edge_id <<
          " to_edge=" << trajectory[i].edge_id;
        *diagnostic = out.str();
      }
      return false;
    }
  }
  return true;
}

void apply_semantic_speed_limits(Trajectory & trajectory, const std::vector<SemanticArea> & areas)
{
  for (auto & waypoint : trajectory) {
    for (const auto & area : areas) {
      if (area.speed_limit_mps <= 0.0 ||
        !(area.type == "speed_zone" || area.speed_limit_mps > 0.0)) {continue;}
      if (point_in_polygon({waypoint.x_m, waypoint.y_m, waypoint.yaw_rad}, area.polygon)) {
        waypoint.target_speed_mps = std::min(waypoint.target_speed_mps, area.speed_limit_mps);
        waypoint.behavior = "semantic_speed_zone:" + area.id;
      }
    }
  }
}

void recompute_route_summary(PlanResult & route, const Trajectory & full_reference)
{
  route.length_m = 0.0;
  route.estimated_time_s = 0.0;
  for (std::size_t i = 1U; i < full_reference.size(); ++i) {
    const double segment = distance(full_reference[i - 1U], full_reference[i]);
    route.length_m += segment;
    route.estimated_time_s += segment / std::max(full_reference[i - 1U].target_speed_mps, 0.1);
  }
}

Trajectory build_edge_segment_between(
  const RoadnetPackage & package, const RoadnetAnchor & start, const RoadnetAnchor & goal, bool reverse)
{
  Trajectory result;
  if (!anchors_on_same_edge(start, goal)) {return result;}
  const auto range = package.waypoint_index_by_edge.find(goal.edge_id);
  if (range == package.waypoint_index_by_edge.end() || range->second.count == 0U) {return result;}
  const auto begin = range->second.start_index;
  const auto end = std::min(range->second.end_index_exclusive, package.waypoints.size());
  if (end <= begin) {return result;}
  const auto first = std::clamp(start.waypoint_index, begin, end - 1U);
  const auto last = std::clamp(goal.waypoint_index, begin, end - 1U);
  if (reverse) {
    for (std::size_t i = first + 1U; i-- > last;) {
      auto waypoint = package.waypoints[i]; waypoint.gear = 2; waypoint.behavior = "semantic_reverse_local";
      append_unique(result, waypoint);
      if (i == 0U) {break;}
    }
  } else {
    for (std::size_t i = first; i <= last && i < end; ++i) {
      auto waypoint = package.waypoints[i]; waypoint.behavior = "semantic_forward_local";
      append_unique(result, waypoint);
    }
  }
  if (!result.empty()) {
    if (start.has_pose) {result.front().x_m = start.x_m; result.front().y_m = start.y_m; result.front().yaw_rad = start.yaw_rad;}
    if (goal.has_pose) {
      result.back().x_m = goal.x_m; result.back().y_m = goal.y_m; result.back().yaw_rad = goal.yaw_rad;
      result.back().edge_s_m = goal.s_on_edge_m;
      if (!goal.point_id.empty()) {result.back().waypoint_id = goal.point_id;}
    }
    if (goal.require_final_stop) {
      result.back().target_speed_mps = 0.0;
      result.back().behavior = reverse ? "semantic_goal_reverse_stop" : "semantic_goal_stop";
    }
  }
  regenerate_route_s(result);
  return result;
}

void TrajectoryProgressTracker::reset()
{
  trajectory_identity_.clear(); progress_index_ = 0U; initialized_ = false;
}

Trajectory TrajectoryProgressTracker::crop(
  const Trajectory & full_reference, const std::optional<Pose2d> & pose,
  const std::string & trajectory_identity, const ProgressSearchOptions & options)
{
  if (full_reference.empty()) {reset(); return {};}
  if (!initialized_ || trajectory_identity != trajectory_identity_) {
    trajectory_identity_ = trajectory_identity; progress_index_ = 0U; initialized_ = true;
  }
  if (pose) {
    const auto begin = progress_index_ > options.backward_window_points ?
      progress_index_ - options.backward_window_points : 0U;
    const auto end = std::min(full_reference.size(), progress_index_ + options.forward_window_points + 1U);
    auto best = progress_index_;
    double best_distance = std::numeric_limits<double>::infinity();
    for (std::size_t i = begin; i < end; ++i) {
      const double heading = std::fabs(angle_difference(pose->yaw_rad, full_reference[i].yaw_rad));
      if (heading > options.max_heading_error_rad) {continue;}
      const double d = std::hypot(pose->x_m - full_reference[i].x_m, pose->y_m - full_reference[i].y_m);
      if (d < best_distance) {best_distance = d; best = i;}
    }
    progress_index_ = std::max(progress_index_, best);
  }
  Trajectory local;
  const double start_s = full_reference[progress_index_].route_s_m;
  for (std::size_t i = progress_index_; i < full_reference.size(); ++i) {
    if (options.horizon_distance_m > 0.0 &&
      full_reference[i].route_s_m - start_s > options.horizon_distance_m && local.size() > 1U) {break;}
    local.push_back(full_reference[i]);
  }
  regenerate_route_s(local);
  return local;
}

}  // namespace low_speed_av_planning
