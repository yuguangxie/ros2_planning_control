#include "low_speed_av_planning/planning_node.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <exception>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "low_speed_av_planning/global_planner_factory.hpp"
#include "low_speed_av_planning/motion_planner_base.hpp"
#include "low_speed_av_planning/planning_helpers.hpp"
#include "low_speed_av_planning/speed_planner_base.hpp"
#include "low_speed_av_planning/topology_graph.hpp"

namespace low_speed_av_planning {
namespace {

double normalize_angle(double angle)
{
  constexpr double pi = 3.14159265358979323846;
  while (angle > pi) {
    angle -= 2.0 * pi;
  }
  while (angle < -pi) {
    angle += 2.0 * pi;
  }
  return angle;
}

double yaw_from_quaternion(const geometry_msgs::msg::Quaternion & q)
{
  const double siny_cosp = 2.0 * (q.w * q.z + q.x * q.y);
  const double cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
  return std::atan2(siny_cosp, cosy_cosp);
}

double squared_distance(const Pose2d & a, const Waypoint & b)
{
  const double dx = a.x_m - b.x_m;
  const double dy = a.y_m - b.y_m;
  return dx * dx + dy * dy;
}

double distance_xy(double ax, double ay, double bx, double by)
{
  return std::hypot(ax - bx, ay - by);
}

double waypoint_distance(const Waypoint & a, const Waypoint & b)
{
  return distance_xy(a.x_m, a.y_m, b.x_m, b.y_m);
}

std::chrono::nanoseconds period_from_rate(double rate_hz)
{
  if (rate_hz <= 0.0 || !std::isfinite(rate_hz)) {
    return std::chrono::nanoseconds(0);
  }
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(1.0 / rate_hz));
}

std::string compose_success_message(const std::string & start_diagnostic, const std::string & goal_diagnostic)
{
  std::ostringstream oss;
  oss << "ok";
  if (!start_diagnostic.empty()) {
    oss << "; start: " << start_diagnostic;
  }
  if (!goal_diagnostic.empty()) {
    oss << "; goal: " << goal_diagnostic;
  }
  return oss.str();
}

std::string compose_resolution_failure_message(
  bool start_failed,
  bool goal_failed,
  const std::string & start_diagnostic,
  const std::string & goal_diagnostic)
{
  std::ostringstream oss;
  if (goal_failed && !goal_diagnostic.empty()) {
    oss << "goal resolution failed: " << goal_diagnostic;
    if (!start_diagnostic.empty()) {
      oss << "; start: " << start_diagnostic;
    }
    return oss.str();
  }
  if (start_failed && !start_diagnostic.empty()) {
    oss << "start resolution failed: " << start_diagnostic;
    if (!goal_diagnostic.empty()) {
      oss << "; goal: " << goal_diagnostic;
    }
    return oss.str();
  }
  return "mission start or goal cannot be resolved";
}

bool any_of_type(const std::string & value, std::initializer_list<const char *> accepted)
{
  return std::any_of(accepted.begin(), accepted.end(), [&](const char * item) {
    return value == item;
  });
}

PlanResult route_with_goal_edge_for_message(PlanResult route, const RoadnetAnchor & goal_anchor)
{
  if (!route.success || !goal_anchor.has_edge || goal_anchor.edge_id.empty()) {
    return route;
  }
  if (std::find(route.edge_ids.begin(), route.edge_ids.end(), goal_anchor.edge_id) == route.edge_ids.end()) {
    route.edge_ids.push_back(goal_anchor.edge_id);
  }
  if (!goal_anchor.edge_to_node_id.empty() &&
    (route.node_ids.empty() || route.node_ids.back() != goal_anchor.edge_to_node_id))
  {
    route.node_ids.push_back(goal_anchor.edge_to_node_id);
  }
  return route;
}

}  // namespace

PlanningNode::PlanningNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("low_speed_av_planning", options)
{
  declare_parameter<std::string>("roadnet.package_path", "");
  declare_parameter<bool>("roadnet.reject_failed_validation", true);
  declare_parameter<bool>("roadnet.verify_checksums", true);
  declare_parameter<std::string>("topics.localization_pose_topic", "/localization/pose");
  declare_parameter<std::string>("topics.trajectory_topic", "/planning/trajectory");
  declare_parameter<std::string>("topics.global_route_topic", "/planning/global_route");
  declare_parameter<std::string>("topics.planning_status_topic", "/planning/status");
  declare_parameter<std::string>("topics.roadnet_status_topic", "/planning/roadnet_status");
  declare_parameter<std::string>("planning.full_reference_path_topic", "/planning/full_reference_path");
  declare_parameter<std::string>("global_planner.algorithm", "astar");
  declare_parameter<double>("global_planner.heuristic_weight", 1.0);
  declare_parameter<bool>("global_planner.allow_reverse", true);
  declare_parameter<std::vector<std::string>>(
    "global_planner.blocked_edges", std::vector<std::string>{});
  declare_parameter<std::string>("motion_planner.algorithm", "reference_line");
  declare_parameter<double>("motion_planner.horizon_distance_m", 15.0);
  declare_parameter<bool>("motion_planner.deduplicate_edge_boundary_points", true);
  declare_parameter<bool>("motion_planner.regenerate_route_s", true);
  declare_parameter<std::string>("speed_planner.algorithm", "curvature");
  declare_parameter<double>("speed_planner.default_speed_mps", 0.5);
  declare_parameter<double>("speed_planner.max_speed_mps", 1.0);
  declare_parameter<double>("speed_planner.max_lateral_accel_mps2", 0.5);
  declare_parameter<double>("speed_planner.obstacle_distance_m", -1.0);
  declare_parameter<double>("speed_planner.obstacle_stop_distance_m", 2.0);
  declare_parameter<double>("planning.localization_timeout_s", 1.0);
  declare_parameter<bool>("planning.use_current_pose_as_start", true);
  declare_parameter<double>("planning.start_match_max_distance_m", 3.0);
  declare_parameter<double>("planning.start_match_max_heading_error_rad", 1.57);
  declare_parameter<bool>("planning.start_match_prefer_edge_projection", true);
  declare_parameter<bool>("planning.republish_last_route", true);
  declare_parameter<bool>("planning.republish_last_trajectory", true);
  declare_parameter<double>("planning.route_republish_rate_hz", 1.0);
  declare_parameter<double>("planning.trajectory_republish_rate_hz", 10.0);
  declare_parameter<double>("planning.roadnet_status_publish_rate_hz", 1.0);
  declare_parameter<double>("planning.arrival_radius_m", 0.5);
  declare_parameter<double>("planning.arrival_heading_tolerance_rad", 0.35);
  declare_parameter<bool>("planning.semantic_goal_use_edge_projection", true);
  declare_parameter<bool>("planning.semantic_goal_allow_reverse_local_segment", false);
  declare_parameter<bool>("planning.semantic_goal_crop_waypoints", true);
  declare_parameter<double>("planning.semantic_goal_min_segment_length_m", 0.2);
  declare_parameter<bool>("planning.reverse.allow_reverse_planning", false);
  declare_parameter<bool>("planning.reverse.allow_reverse_local_segment", false);
  declare_parameter<bool>("planning.reverse.require_reverse_confirmation", true);
  declare_parameter<bool>("planning.reverse.prefer_forward_route_when_reverse_disabled", true);
  declare_parameter<bool>("planning.reverse.fail_if_goal_behind_on_same_edge_when_reverse_disabled", false);
  declare_parameter<bool>("planning.start_anchor.include_current_edge_prefix", true);
  declare_parameter<double>("planning.start_anchor.max_start_projection_distance_m", 2.0);
  declare_parameter<double>("planning.start_anchor.max_first_trajectory_point_distance_m", 2.0);
  declare_parameter<bool>("planning.publish_full_reference_path", true);
  declare_parameter<bool>("planning.local_trajectory_from_current_pose", true);
  declare_parameter<double>("planning.max_trajectory_point_jump_m", 2.0);
  declare_parameter<int>("planning.progress_search_backward_points", 2);
  declare_parameter<int>("planning.progress_search_forward_points", 200);
  declare_parameter<double>("planning.progress_max_heading_error_rad", 1.57);

  global_planner_algorithm_ = get_parameter("global_planner.algorithm").as_string();
  motion_planner_algorithm_ = get_parameter("motion_planner.algorithm").as_string();
  speed_planner_algorithm_ = get_parameter("speed_planner.algorithm").as_string();

  global_route_pub_ = create_publisher<low_speed_av_interfaces::msg::GlobalRoute>(
    get_parameter("topics.global_route_topic").as_string(), 10);
  trajectory_pub_ = create_publisher<low_speed_av_interfaces::msg::Trajectory>(
    get_parameter("topics.trajectory_topic").as_string(), 10);
  full_reference_path_pub_ = create_publisher<low_speed_av_interfaces::msg::Trajectory>(
    get_parameter("planning.full_reference_path_topic").as_string(), 10);
  planning_status_pub_ = create_publisher<low_speed_av_interfaces::msg::ModuleStatus>(
    get_parameter("topics.planning_status_topic").as_string(), 10);
  const auto roadnet_status_qos = rclcpp::QoS(1).transient_local().reliable();
  roadnet_status_pub_ = create_publisher<low_speed_av_interfaces::msg::RoadnetStatus>(
    get_parameter("topics.roadnet_status_topic").as_string(), roadnet_status_qos);
  pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
    get_parameter("topics.localization_pose_topic").as_string(), 10,
    [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
      on_localization_pose(msg);
    });

  reload_srv_ = create_service<low_speed_av_interfaces::srv::ReloadRoadnet>(
    "~/reload_roadnet",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Response> response) {
      on_reload_roadnet(request, response);
    });
  plan_route_srv_ = create_service<low_speed_av_interfaces::srv::PlanRoute>(
    "~/plan_route",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Response> response) {
      on_plan_route(request, response);
    });
  plan_mission_srv_ = create_service<low_speed_av_interfaces::srv::PlanMission>(
    "~/plan_mission",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Response> response) {
      on_plan_mission(request, response);
    });
  set_algorithm_srv_ = create_service<low_speed_av_interfaces::srv::SetPlannerAlgorithm>(
    "~/set_planner_algorithm",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Response> response) {
      on_set_planner_algorithm(request, response);
    });

  const auto route_period = period_from_rate(get_parameter("planning.route_republish_rate_hz").as_double());
  if (route_period.count() > 0) {
    route_republish_timer_ = create_wall_timer(route_period, [this]() { republish_last_route(); });
  }
  const auto trajectory_period =
    period_from_rate(get_parameter("planning.trajectory_republish_rate_hz").as_double());
  if (trajectory_period.count() > 0) {
    trajectory_republish_timer_ =
      create_wall_timer(trajectory_period, [this]() { republish_last_trajectory(); });
  }
  const auto roadnet_status_period =
    period_from_rate(get_parameter("planning.roadnet_status_publish_rate_hz").as_double());
  if (roadnet_status_period.count() > 0) {
    roadnet_status_timer_ =
      create_wall_timer(roadnet_status_period, [this]() { republish_last_roadnet_status(); });
  }

  load_package_from_parameter();
}

void PlanningNode::load_package_from_parameter()
{
  const auto package_path = get_parameter("roadnet.package_path").as_string();
  if (package_path.empty()) {
    RCLCPP_WARN(get_logger(), "roadnet.package_path is empty; planning waits for ReloadRoadnet");
    publish_status("waiting", "roadnet.package_path is empty");
    publish_roadnet_status(false, "roadnet.package_path is empty");
    return;
  }
  try {
    RoadnetLoader::Options options;
    options.reject_failed_validation = get_parameter("roadnet.reject_failed_validation").as_bool();
    options.verify_checksums = get_parameter("roadnet.verify_checksums").as_bool();
    package_ = std::make_shared<RoadnetPackage>(loader_.load(package_path, options));
    clear_cached_plan();
    RCLCPP_INFO(get_logger(), "loaded AD package %s", package_->package_id.c_str());
    publish_roadnet_status(true, "roadnet ready");
    publish_status("active", "roadnet ready");
  } catch (const std::exception & e) {
    package_.reset();
    clear_cached_plan();
    RCLCPP_ERROR(get_logger(), "planning inactive: %s", e.what());
    publish_roadnet_status(false, e.what());
    publish_status("failure", e.what());
  }
}

void PlanningNode::publish_status(const std::string & state, const std::string & message)
{
  low_speed_av_interfaces::msg::ModuleStatus status;
  status.header.stamp = now();
  status.module_name = "low_speed_av_planning";
  status.state = state;
  status.level = state == "failure" ? 2 : state == "warning" ? 1 : 0;
  status.message = message;
  planning_status_pub_->publish(status);
}

void PlanningNode::publish_roadnet_status(bool ready, const std::string & message)
{
  low_speed_av_interfaces::msg::RoadnetStatus status;
  status.header.stamp = now();
  if (package_) {
    status.package_id = package_->package_id;
    status.schema_version = package_->schema_version;
    status.nodes = static_cast<uint32_t>(package_->nodes.size());
    status.edges = static_cast<uint32_t>(package_->edges.size());
    status.waypoints = static_cast<uint32_t>(package_->waypoints.size());
    status.validation_status = ready ? "passed" : "unknown";
  }
  status.ready = ready;
  status.message = message;
  last_roadnet_status_msg_ = status;
  has_last_roadnet_status_msg_ = true;
  roadnet_status_pub_->publish(status);
}

void PlanningNode::republish_last_route()
{
  if (!get_parameter("planning.republish_last_route").as_bool() || !has_last_route_msg_) {
    return;
  }
  auto msg = last_route_msg_;
  msg.header.stamp = now();
  global_route_pub_->publish(msg);
}

void PlanningNode::republish_last_trajectory()
{
  if (!get_parameter("planning.republish_last_trajectory").as_bool() || !has_last_trajectory_msg_) {
    return;
  }
  if (has_last_full_reference_path_msg_ &&
    get_parameter("planning.local_trajectory_from_current_pose").as_bool())
  {
    auto local = make_local_trajectory_from_full_reference(last_full_reference_path_);
    std::string continuity_error;
    if (!trajectory_is_continuous(local, &continuity_error)) {
      RCLCPP_WARN_THROTTLE(
        get_logger(), *get_clock(), 2000, "not republishing discontinuous local trajectory: %s",
        continuity_error.c_str());
      return;
    }
    last_trajectory_msg_ = to_msg(local, "ok");
  }
  auto msg = last_trajectory_msg_;
  msg.header.stamp = now();
  for (auto & point : msg.points) {
    point.header = msg.header;
  }
  trajectory_pub_->publish(msg);
  republish_last_full_reference_path();
}

void PlanningNode::republish_last_full_reference_path()
{
  if (!get_parameter("planning.publish_full_reference_path").as_bool() || !has_last_full_reference_path_msg_) {
    return;
  }
  auto msg = last_full_reference_path_msg_;
  msg.header.stamp = now();
  for (auto & point : msg.points) {
    point.header = msg.header;
  }
  full_reference_path_pub_->publish(msg);
}

void PlanningNode::republish_last_roadnet_status()
{
  if (!has_last_roadnet_status_msg_) {
    return;
  }
  auto msg = last_roadnet_status_msg_;
  msg.header.stamp = now();
  roadnet_status_pub_->publish(msg);
}

void PlanningNode::clear_cached_plan()
{
  has_last_route_msg_ = false;
  has_last_trajectory_msg_ = false;
  has_last_full_reference_path_msg_ = false;
  last_full_reference_path_.clear();
  progress_tracker_.reset();
}

void PlanningNode::publish_failure_trajectory(const std::string & reason)
{
  has_last_route_msg_ = false;
  has_last_full_reference_path_msg_ = false;
  last_full_reference_path_.clear();
  low_speed_av_interfaces::msg::Trajectory msg;
  msg.header.stamp = now();
  msg.trajectory_id = "failure_stop";
  msg.source_package_id = package_ ? package_->package_id : "";
  msg.planner_algorithm = global_planner_algorithm_ + "/" + motion_planner_algorithm_ + "/" + speed_planner_algorithm_;
  msg.emergency_stop = true;
  msg.status = reason;
  if (package_ && !package_->waypoints.empty()) {
    const auto & wp = package_->waypoints.front();
    low_speed_av_interfaces::msg::TrajectoryPoint point;
    point.header = msg.header;
    point.index = 0;
    point.waypoint_id = wp.waypoint_id;
    point.edge_id = wp.edge_id;
    point.path_id = wp.path_id;
    point.x_m = wp.x_m;
    point.y_m = wp.y_m;
    point.yaw_rad = wp.yaw_rad;
    point.kappa_1pm = 0.0;
    point.s_m = 0.0;
    point.v_mps = 0.0;
    point.a_mps2 = 0.0;
    point.relative_time_s = 0.0;
    point.gear = static_cast<int8_t>(wp.gear);
    point.behavior = "planning_failure_stop";
    msg.points.push_back(point);
  }
  last_trajectory_msg_ = msg;
  has_last_trajectory_msg_ = true;
  trajectory_pub_->publish(msg);
}

GlobalPlannerOptions PlanningNode::read_global_options() const
{
  GlobalPlannerOptions options;
  options.allow_reverse =
    get_parameter("global_planner.allow_reverse").as_bool() && reverse_planning_allowed();
  options.heuristic_weight = get_parameter("global_planner.heuristic_weight").as_double();
  for (const auto & edge_id : get_parameter("global_planner.blocked_edges").as_string_array()) {
    options.blocked_edges.insert(edge_id);
  }
  if (package_) {
    options.blocked_edges.insert(package_->blocked_edges.begin(), package_->blocked_edges.end());
  }
  return options;
}

bool PlanningNode::reverse_planning_allowed() const
{
  return get_parameter("planning.reverse.allow_reverse_planning").as_bool();
}

bool PlanningNode::reverse_local_segment_allowed() const
{
  return reverse_planning_allowed() &&
    get_parameter("planning.reverse.allow_reverse_local_segment").as_bool();
}

bool PlanningNode::start_and_goal_on_same_edge(
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor) const
{
  return anchors_on_same_edge(start_anchor, goal_anchor);
}

bool PlanningNode::goal_is_behind_start_on_same_edge(
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor) const
{
  return goal_behind_on_same_edge(start_anchor, goal_anchor);
}

RouteDecision PlanningNode::plan_route_between_anchors(
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor)
{
  RouteDecision decision;
  if (!package_) {
    decision.route = {false, "roadnet package not loaded", global_planner_algorithm_, {}, {}, 0.0, 0.0};
    return decision;
  }

  if (start_and_goal_on_same_edge(start_anchor, goal_anchor)) {
    const bool goal_behind = goal_is_behind_start_on_same_edge(start_anchor, goal_anchor);
    if (!goal_behind) {
      decision.route = {
        true, "same edge forward segment", global_planner_algorithm_,
        {start_anchor.node_id}, {}, 0.0, 0.0};
      decision.note = "same edge forward segment selected";
      return decision;
    }

    if (reverse_local_segment_allowed()) {
      decision.route = {
        true, "same edge reverse local segment", global_planner_algorithm_,
        {start_anchor.node_id}, {}, 0.0, 0.0};
      decision.note = "reverse local segment selected";
      return decision;
    }

    if (get_parameter("planning.reverse.fail_if_goal_behind_on_same_edge_when_reverse_disabled").as_bool()) {
      decision.route = {
        false, "reverse planning is disabled and same-edge goal is behind current pose",
        global_planner_algorithm_, {}, {}, 0.0, 0.0};
      decision.note = "reverse disabled; no forward detour attempted";
      return decision;
    }

    if (!goal_anchor.edge_from_node_id.empty()) {
      decision.route = compute_route(start_anchor.node_id, goal_anchor.edge_from_node_id);
      if (decision.route.success) {
        decision.note = "reverse disabled; using forward detour";
        return decision;
      }
    }
    decision.route = {
      false, "reverse planning is disabled and forward route to goal is unavailable",
      global_planner_algorithm_, {}, {}, 0.0, 0.0};
    decision.note = "reverse disabled; no forward detour available";
    return decision;
  }

  const bool prefer_goal_edge_from = goal_anchor.has_edge && !goal_anchor.edge_from_node_id.empty();
  if (prefer_goal_edge_from) {
    decision.route = compute_route(start_anchor.node_id, goal_anchor.edge_from_node_id);
    if (decision.route.success) {
      return decision;
    }
    decision.route = compute_route(start_anchor.node_id, goal_anchor.node_id);
    return decision;
  }

  decision.route = compute_route(start_anchor.node_id, goal_anchor.node_id);
  if (!decision.route.success && goal_anchor.has_edge && !goal_anchor.edge_from_node_id.empty()) {
    const auto route_to_edge_from = compute_route(start_anchor.node_id, goal_anchor.edge_from_node_id);
    if (route_to_edge_from.success) {
      decision.route = route_to_edge_from;
    }
  }
  return decision;
}

MotionPlannerOptions PlanningNode::read_motion_options() const
{
  MotionPlannerOptions options;
  options.horizon_distance_m = get_parameter("motion_planner.horizon_distance_m").as_double();
  options.deduplicate_edge_boundary_points =
    get_parameter("motion_planner.deduplicate_edge_boundary_points").as_bool();
  options.regenerate_route_s = get_parameter("motion_planner.regenerate_route_s").as_bool();
  return options;
}

SpeedPlannerOptions PlanningNode::read_speed_options() const
{
  SpeedPlannerOptions options;
  options.default_speed_mps = get_parameter("speed_planner.default_speed_mps").as_double();
  options.max_speed_mps = get_parameter("speed_planner.max_speed_mps").as_double();
  options.max_lateral_accel_mps2 = get_parameter("speed_planner.max_lateral_accel_mps2").as_double();
  options.obstacle_distance_m = get_parameter("speed_planner.obstacle_distance_m").as_double();
  options.obstacle_stop_distance_m = get_parameter("speed_planner.obstacle_stop_distance_m").as_double();
  return options;
}

PlanResult PlanningNode::compute_route(const std::string & start_node_id, const std::string & goal_node_id)
{
  if (!package_) {
    return {false, "roadnet package not loaded", global_planner_algorithm_, {}, {}, 0.0, 0.0};
  }
  TopologyGraph graph(*package_);
  if (!graph.node(start_node_id) || !graph.node(goal_node_id)) {
    return {false, "start or goal node is not in topology", global_planner_algorithm_, {}, {}, 0.0, 0.0};
  }
  const auto planner = GlobalPlannerFactory::create(global_planner_algorithm_);
  return planner->plan(graph, start_node_id, goal_node_id, read_global_options());
}

Trajectory PlanningNode::compute_trajectory(const PlanResult & route)
{
  if (!package_ || !route.success) {
    return {};
  }
  const auto motion = MotionPlannerFactory::create(motion_planner_algorithm_);
  auto trajectory = motion->make_trajectory(*package_, route.edge_ids, nullptr, read_motion_options());
  const auto speed = SpeedPlannerFactory::create(speed_planner_algorithm_);
  speed->apply(trajectory, read_speed_options());
  apply_semantic_speed_limits(trajectory);
  return trajectory;
}

Trajectory PlanningNode::compute_full_reference_path_to_goal_anchor(
  const PlanResult & route,
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor)
{
  if (!package_ || !route.success) {
    return {};
  }

  if (start_anchor.node_id == goal_anchor.node_id && has_arrived(goal_anchor)) {
    return make_arrived_stop_trajectory(goal_anchor, "arrived_stop");
  }

  MotionPlannerOptions full_options = read_motion_options();
  full_options.horizon_distance_m = 0.0;
  const auto motion = MotionPlannerFactory::create(motion_planner_algorithm_);
  Trajectory full_reference;
  if (start_and_goal_on_same_edge(start_anchor, goal_anchor) && goal_anchor.has_edge) {
    const bool reverse_segment = goal_is_behind_start_on_same_edge(start_anchor, goal_anchor);
    if (reverse_segment && reverse_local_segment_allowed()) {
      append_edge_segment_between(full_reference, start_anchor, goal_anchor, true);
    } else if (!reverse_segment) {
      append_edge_segment_between(full_reference, start_anchor, goal_anchor, false);
    }
  }

  if (full_reference.empty()) {
    append_start_edge_remaining_segment(full_reference, start_anchor);
    auto route_reference = motion->make_trajectory(*package_, route.edge_ids, nullptr, full_options);
    for (auto & wp : route_reference) {
      append_waypoint(full_reference, wp);
    }
  }

  const bool semantic_crop = get_parameter("planning.semantic_goal_crop_waypoints").as_bool();

  if (semantic_crop && goal_anchor.has_edge) {
    const bool reverse_goal_segment =
      (!route.node_ids.empty() && route.node_ids.back() == goal_anchor.edge_to_node_id &&
      reverse_local_segment_allowed()) ||
      (route.edge_ids.empty() && start_anchor.node_id == goal_anchor.edge_to_node_id &&
      reverse_local_segment_allowed());
    if (!(start_and_goal_on_same_edge(start_anchor, goal_anchor) && !full_reference.empty())) {
      append_edge_segment(full_reference, goal_anchor, reverse_goal_segment);
    }
  }

  const double min_length = get_parameter("planning.semantic_goal_min_segment_length_m").as_double();
  if (full_reference.size() == 1U && goal_anchor.has_pose && start_anchor.has_pose &&
    distance_xy(start_anchor.x_m, start_anchor.y_m, goal_anchor.x_m, goal_anchor.y_m) > min_length)
  {
    auto start = full_reference.front();
    start.x_m = start_anchor.x_m;
    start.y_m = start_anchor.y_m;
    start.yaw_rad = start_anchor.yaw_rad;
    full_reference.insert(full_reference.begin(), start);
  }

  regenerate_route_s(full_reference);
  const auto speed = SpeedPlannerFactory::create(speed_planner_algorithm_);
  speed->apply(full_reference, read_speed_options());
  if (!full_reference.empty() && goal_anchor.require_final_stop) {
    full_reference.back().target_speed_mps = 0.0;
  }
  apply_semantic_speed_limits(full_reference);
  return full_reference;
}

Trajectory PlanningNode::make_local_trajectory_from_full_reference(const Trajectory & full_reference)
{
  ProgressSearchOptions options;
  options.horizon_distance_m = get_parameter("motion_planner.horizon_distance_m").as_double();
  options.backward_window_points = static_cast<std::size_t>(std::max(
    get_parameter("planning.progress_search_backward_points").as_int(), static_cast<int64_t>(0)));
  options.forward_window_points = static_cast<std::size_t>(std::max(
    get_parameter("planning.progress_search_forward_points").as_int(), static_cast<int64_t>(1)));
  options.max_heading_error_rad = get_parameter("planning.progress_max_heading_error_rad").as_double();
  const auto pose = get_parameter("planning.local_trajectory_from_current_pose").as_bool() ?
    latest_pose_ : std::optional<Pose2d>{};
  std::string identity = package_ ? package_->package_id : "no_package";
  if (!full_reference.empty()) {
    identity += ":" + full_reference.front().waypoint_id + ":" + full_reference.back().waypoint_id +
      ":" + std::to_string(full_reference.size());
  }
  return progress_tracker_.crop(full_reference, pose, identity, options);
}

bool PlanningNode::trajectory_is_continuous(const Trajectory & trajectory, std::string * diagnostic) const
{
  const double max_jump = get_parameter("planning.max_trajectory_point_jump_m").as_double();
  return low_speed_av_planning::trajectory_is_continuous(trajectory, max_jump, diagnostic);
}

std::optional<RoadnetAnchor> PlanningNode::make_node_anchor(
  const std::string & node_id,
  RoadnetAnchor::Type type,
  const std::string & point_id,
  std::string * diagnostic) const
{
  if (!package_) {
    if (diagnostic) {
      *diagnostic = "roadnet package not loaded";
    }
    return std::nullopt;
  }
  return low_speed_av_planning::make_node_anchor(*package_, node_id, type, point_id, diagnostic);
}

bool PlanningNode::project_anchor_to_edge(RoadnetAnchor & anchor, std::string * diagnostic) const
{
  if (!package_) {
    return false;
  }
  return low_speed_av_planning::project_anchor_to_edge(*package_, anchor, diagnostic);
}

std::optional<RoadnetAnchor> PlanningNode::make_semantic_anchor(
  const std::string & point_id,
  const std::map<std::string, SemanticPoint> & points,
  const std::string & point_kind,
  RoadnetAnchor::Type type,
  bool prefer_edge_to_node,
  std::string * diagnostic) const
{
  if (!package_) {
    if (diagnostic) {
      *diagnostic = "roadnet package not loaded";
    }
    return std::nullopt;
  }
  return low_speed_av_planning::make_semantic_anchor(
    *package_, point_id, points, point_kind, type, prefer_edge_to_node, diagnostic);
}

std::optional<RoadnetAnchor> PlanningNode::match_current_pose_to_start_anchor(std::string * diagnostic) const
{
  const auto matched_node = match_current_pose_to_start_node(diagnostic);
  if (!matched_node || !package_ || !latest_pose_) {
    return std::nullopt;
  }
  return low_speed_av_planning::make_current_pose_anchor(
    *package_, *latest_pose_, *matched_node,
    get_parameter("planning.start_match_max_heading_error_rad").as_double(),
    get_parameter("planning.start_anchor.max_start_projection_distance_m").as_double(),
    get_parameter("planning.start_anchor.include_current_edge_prefix").as_bool(), diagnostic);
}

std::optional<RoadnetAnchor> PlanningNode::resolve_start_anchor(
  const std::string & node_id,
  const std::string & task_point_id,
  std::string * diagnostic) const
{
  if (!node_id.empty()) {
    return make_node_anchor(node_id, RoadnetAnchor::Type::Node, node_id, diagnostic);
  }
  if (package_ && !task_point_id.empty()) {
    return make_semantic_anchor(
      task_point_id, package_->task_points, "task point", RoadnetAnchor::Type::TaskPoint, false, diagnostic);
  }
  if (!get_parameter("planning.use_current_pose_as_start").as_bool()) {
    if (diagnostic) {
      *diagnostic = "start node is empty and planning.use_current_pose_as_start is false";
    }
    return std::nullopt;
  }
  return match_current_pose_to_start_anchor(diagnostic);
}

std::optional<RoadnetAnchor> PlanningNode::resolve_goal_anchor(
  const std::string & node_id,
  const std::string & task_point_id,
  const std::string & parking_point_id,
  std::string * diagnostic) const
{
  if (!node_id.empty()) {
    return make_node_anchor(node_id, RoadnetAnchor::Type::Node, node_id, diagnostic);
  }
  if (package_ && !task_point_id.empty()) {
    return make_semantic_anchor(
      task_point_id, package_->task_points, "task point", RoadnetAnchor::Type::TaskPoint, true, diagnostic);
  }
  if (package_ && !parking_point_id.empty()) {
    return make_semantic_anchor(
      parking_point_id, package_->parking_points, "parking point", RoadnetAnchor::Type::ParkingPoint, true,
      diagnostic);
  }
  if (diagnostic) {
    *diagnostic = "goal node or semantic goal is empty";
  }
  return std::nullopt;
}

bool PlanningNode::has_arrived(const RoadnetAnchor & goal_anchor) const
{
  if (!latest_pose_ || !goal_anchor.has_pose) {
    return false;
  }
  const double distance = distance_xy(latest_pose_->x_m, latest_pose_->y_m, goal_anchor.x_m, goal_anchor.y_m);
  const double heading_error = std::fabs(normalize_angle(latest_pose_->yaw_rad - goal_anchor.yaw_rad));
  return distance <= get_parameter("planning.arrival_radius_m").as_double() &&
    heading_error <= get_parameter("planning.arrival_heading_tolerance_rad").as_double();
}

void PlanningNode::append_waypoint(Trajectory & trajectory, Waypoint waypoint) const
{
  if (!trajectory.empty() && waypoint_distance(trajectory.back(), waypoint) < 1e-4) {
    return;
  }
  trajectory.push_back(std::move(waypoint));
}

void PlanningNode::regenerate_route_s(Trajectory & trajectory) const
{
  low_speed_av_planning::regenerate_route_s(trajectory);
}

Trajectory PlanningNode::make_arrived_stop_trajectory(
  const RoadnetAnchor & anchor,
  const std::string & behavior) const
{
  Trajectory trajectory;
  Waypoint point;
  point.index = 0;
  point.waypoint_id = anchor.point_id.empty() ? "arrived_stop" : anchor.point_id;
  point.edge_id = anchor.edge_id;
  point.x_m = anchor.x_m;
  point.y_m = anchor.y_m;
  point.yaw_rad = anchor.yaw_rad;
  point.kappa_1pm = 0.0;
  point.edge_s_m = anchor.s_on_edge_m;
  point.route_s_m = 0.0;
  point.target_speed_mps = 0.0;
  point.gear = 1;
  point.behavior = behavior;
  trajectory.push_back(point);
  return trajectory;
}

void PlanningNode::append_edge_segment(
  Trajectory & trajectory,
  const RoadnetAnchor & goal_anchor,
  bool reverse) const
{
  if (!package_ || !goal_anchor.has_edge) {
    return;
  }
  const auto range_it = package_->waypoint_index_by_edge.find(goal_anchor.edge_id);
  if (range_it == package_->waypoint_index_by_edge.end() ||
    range_it->second.count == 0U || package_->waypoints.empty())
  {
    return;
  }
  const auto start = range_it->second.start_index;
  const auto end = std::min(range_it->second.end_index_exclusive, package_->waypoints.size());
  const auto goal_index = std::min(std::max(goal_anchor.waypoint_index, start), end - 1U);
  if (reverse) {
    for (std::size_t i = end; i-- > goal_index;) {
      auto wp = package_->waypoints[i];
      wp.gear = 2;
      wp.behavior = "semantic_reverse_local";
      append_waypoint(trajectory, wp);
      if (i == 0U) {
        break;
      }
    }
  } else {
    for (std::size_t i = start; i <= goal_index && i < end; ++i) {
      auto wp = package_->waypoints[i];
      wp.behavior = "semantic_forward_local";
      append_waypoint(trajectory, wp);
      if (i == std::numeric_limits<std::size_t>::max()) {
        break;
      }
    }
  }

  if (!trajectory.empty() && goal_anchor.has_pose) {
    auto & last = trajectory.back();
    last.waypoint_id = goal_anchor.point_id.empty() ? last.waypoint_id : goal_anchor.point_id;
    last.x_m = goal_anchor.x_m;
    last.y_m = goal_anchor.y_m;
    last.yaw_rad = goal_anchor.yaw_rad;
    last.edge_s_m = goal_anchor.s_on_edge_m;
    if (goal_anchor.require_final_stop) {
      last.target_speed_mps = 0.0;
      last.behavior = reverse ? "semantic_goal_reverse_stop" : "semantic_goal_stop";
    }
  }
}

void PlanningNode::append_edge_segment_between(
  Trajectory & trajectory,
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor,
  bool reverse) const
{
  if (!package_) {return;}
  for (auto waypoint : build_edge_segment_between(*package_, start_anchor, goal_anchor, reverse)) {
    append_waypoint(trajectory, std::move(waypoint));
  }
}

void PlanningNode::append_start_edge_remaining_segment(
  Trajectory & trajectory,
  const RoadnetAnchor & start_anchor) const
{
  if (!package_ || !start_anchor.has_edge ||
    !get_parameter("planning.start_anchor.include_current_edge_prefix").as_bool())
  {
    return;
  }
  const auto range_it = package_->waypoint_index_by_edge.find(start_anchor.edge_id);
  if (range_it == package_->waypoint_index_by_edge.end() ||
    range_it->second.count == 0U || package_->waypoints.empty())
  {
    return;
  }
  const auto edge_end = std::min(range_it->second.end_index_exclusive, package_->waypoints.size());
  const auto start_index =
    std::min(std::max(start_anchor.waypoint_index, range_it->second.start_index), edge_end - 1U);
  for (std::size_t i = start_index; i < edge_end; ++i) {
    auto wp = package_->waypoints[i];
    if (i == start_index && start_anchor.has_pose) {
      wp.x_m = start_anchor.x_m;
      wp.y_m = start_anchor.y_m;
      wp.yaw_rad = start_anchor.yaw_rad;
      wp.edge_s_m = start_anchor.s_on_edge_m;
    }
    wp.gear = 1;
    wp.behavior = "start_anchor_forward_prefix";
    append_waypoint(trajectory, wp);
  }
}

Trajectory PlanningNode::make_edge_segment_trajectory(
  const RoadnetAnchor &,
  const RoadnetAnchor & goal_anchor,
  bool reverse) const
{
  Trajectory trajectory;
  append_edge_segment(trajectory, goal_anchor, reverse);
  regenerate_route_s(trajectory);
  return trajectory;
}

Trajectory PlanningNode::compute_trajectory_to_goal_anchor(
  const PlanResult & route,
  const RoadnetAnchor & start_anchor,
  const RoadnetAnchor & goal_anchor)
{
  auto full_reference = compute_full_reference_path_to_goal_anchor(route, start_anchor, goal_anchor);
  last_full_reference_path_ = full_reference;
  last_full_reference_path_msg_ = to_msg(full_reference, "full_reference_path");
  has_last_full_reference_path_msg_ = !full_reference.empty();
  if (has_last_full_reference_path_msg_ && get_parameter("planning.publish_full_reference_path").as_bool()) {
    full_reference_path_pub_->publish(last_full_reference_path_msg_);
  }
  return make_local_trajectory_from_full_reference(full_reference);
}

void PlanningNode::on_localization_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  Pose2d pose;
  pose.x_m = msg->pose.position.x;
  pose.y_m = msg->pose.position.y;
  pose.yaw_rad = yaw_from_quaternion(msg->pose.orientation);
  if (!std::isfinite(pose.x_m) || !std::isfinite(pose.y_m) || !std::isfinite(pose.yaw_rad)) {
    RCLCPP_WARN(get_logger(), "ignored non-finite localization pose");
    return;
  }
  latest_pose_ = pose;
  latest_pose_receive_time_ = now();
}

void PlanningNode::apply_semantic_speed_limits(Trajectory & trajectory) const
{
  if (!package_) {
    return;
  }
  // The production helper preserves the documented semantic_speed_zone:<area_id> behavior label.
  low_speed_av_planning::apply_semantic_speed_limits(trajectory, package_->areas);
}

std::string PlanningNode::resolve_start_node(
  const std::string & node_id,
  const std::string & task_point_id,
  std::string * diagnostic) const
{
  if (!node_id.empty()) {
    return node_id;
  }
  if (package_ && !task_point_id.empty()) {
    return resolve_semantic_point_node(task_point_id, package_->task_points, "task point", false, diagnostic);
  }
  if (!get_parameter("planning.use_current_pose_as_start").as_bool()) {
    if (diagnostic) {
      *diagnostic = "start node is empty and planning.use_current_pose_as_start is false";
    }
    return "";
  }
  const auto matched = match_current_pose_to_start_node(diagnostic);
  return matched.value_or("");
}

std::optional<std::string> PlanningNode::match_current_pose_to_start_node(std::string * diagnostic) const
{
  if (!package_) {
    if (diagnostic) {
      *diagnostic = "roadnet package not loaded";
    }
    return std::nullopt;
  }
  if (!latest_pose_) {
    if (diagnostic) {
      *diagnostic = "current localization pose is not available";
    }
    return std::nullopt;
  }

  const auto age = (now() - latest_pose_receive_time_).seconds();
  const auto timeout = get_parameter("planning.localization_timeout_s").as_double();
  if (age > timeout) {
    if (diagnostic) {
      std::ostringstream oss;
      oss << "current localization pose is stale: age=" << age << "s timeout=" << timeout << "s";
      *diagnostic = oss.str();
    }
    return std::nullopt;
  }

  const auto max_distance = get_parameter("planning.start_match_max_distance_m").as_double();
  const auto max_heading = get_parameter("planning.start_match_max_heading_error_rad").as_double();
  const bool prefer_edge_projection =
    get_parameter("planning.start_match_prefer_edge_projection").as_bool();

  const Waypoint * best = nullptr;
  double best_distance_sq = std::numeric_limits<double>::infinity();
  double best_heading_error = std::numeric_limits<double>::infinity();
  for (const auto & wp : package_->waypoints) {
    const double heading_error = std::fabs(normalize_angle(latest_pose_->yaw_rad - wp.yaw_rad));
    if (heading_error > max_heading) {
      continue;
    }
    const double dist_sq = squared_distance(*latest_pose_, wp);
    if (dist_sq < best_distance_sq) {
      best = &wp;
      best_distance_sq = dist_sq;
      best_heading_error = heading_error;
    }
  }

  if (!best) {
    if (diagnostic) {
      *diagnostic = "no waypoint matched localization heading threshold";
    }
    return std::nullopt;
  }

  const double distance = std::sqrt(best_distance_sq);
  if (distance > max_distance) {
    if (diagnostic) {
      std::ostringstream oss;
      oss << "nearest roadnet waypoint is too far: distance=" << distance <<
        "m max=" << max_distance << "m waypoint=" << best->waypoint_id;
      *diagnostic = oss.str();
    }
    return std::nullopt;
  }

  const auto edge_it = std::find_if(package_->edges.begin(), package_->edges.end(), [&](const auto & edge) {
    return edge.id == best->edge_id;
  });
  if (edge_it == package_->edges.end()) {
    if (diagnostic) {
      *diagnostic = "matched waypoint references unknown edge: " + best->edge_id;
    }
    return std::nullopt;
  }

  std::string start_node = edge_it->from_node_id;
  if (prefer_edge_projection) {
    const auto range_it = package_->waypoint_index_by_edge.find(best->edge_id);
    if (range_it != package_->waypoint_index_by_edge.end() && range_it->second.count > 1U) {
      const double progress = static_cast<double>(best->index - range_it->second.start_index) /
        static_cast<double>(range_it->second.count - 1U);
      if (progress >= 0.5) {
        start_node = edge_it->to_node_id;
      }
    }
  }

  if (diagnostic) {
    std::ostringstream oss;
    oss << "matched current pose to waypoint=" << best->waypoint_id <<
      " edge=" << best->edge_id <<
      " start_node=" << start_node <<
      " distance_m=" << distance <<
      " heading_error_rad=" << best_heading_error;
    *diagnostic = oss.str();
  }
  return start_node;
}

std::string PlanningNode::resolve_goal_node(
  const std::string & node_id,
  const std::string & task_point_id,
  const std::string & parking_point_id,
  std::string * diagnostic) const
{
  if (!node_id.empty()) {
    return node_id;
  }
  if (package_ && !task_point_id.empty()) {
    return resolve_semantic_point_node(task_point_id, package_->task_points, "task point", true, diagnostic);
  }
  if (package_ && !parking_point_id.empty()) {
    return resolve_semantic_point_node(
      parking_point_id, package_->parking_points, "parking point", true, diagnostic);
  }
  return "";
}

std::string PlanningNode::resolve_semantic_point_node(
  const std::string & point_id,
  const std::map<std::string, SemanticPoint> & points,
  const std::string & point_kind,
  bool prefer_edge_to_node,
  std::string * diagnostic) const
{
  if (!package_) {
    if (diagnostic) {
      *diagnostic = "roadnet package not loaded";
    }
    return "";
  }
  const auto point_it = points.find(point_id);
  if (point_it == points.end()) {
    if (diagnostic) {
      *diagnostic = point_kind + " not found: " + point_id;
    }
    return "";
  }

  const TopologyGraph graph(*package_);
  const auto & point = point_it->second;
  if (!point.linked_node_id.empty()) {
    if (graph.node(point.linked_node_id)) {
      return point.linked_node_id;
    }
    RCLCPP_WARN(
      get_logger(), "%s %s references unknown linked_node_id '%s'; trying linked_edge_id fallback",
      point_kind.c_str(), point_id.c_str(), point.linked_node_id.c_str());
  }

  if (!point.linked_edge_id.empty()) {
    if (const auto * edge = graph.edge(point.linked_edge_id)) {
      return prefer_edge_to_node ? edge->to_node_id : edge->from_node_id;
    }
    if (diagnostic) {
      *diagnostic = point_kind + " " + point_id +
        " linked_edge_id is not in topology: " + point.linked_edge_id;
    }
    return "";
  }

  if (diagnostic) {
    *diagnostic = point_kind + " " + point_id + " has no valid linked_node_id or linked_edge_id";
  }
  return "";
}

low_speed_av_interfaces::msg::GlobalRoute PlanningNode::to_msg(const PlanResult & route) const
{
  low_speed_av_interfaces::msg::GlobalRoute msg;
  msg.header.stamp = now();
  msg.route_id = route.success ? "route_" + route.node_ids.front() + "_" + route.node_ids.back() : "route_failed";
  msg.source_package_id = package_ ? package_->package_id : "";
  msg.planner_algorithm = route.planner_algorithm;
  msg.node_ids = route.node_ids;
  msg.edge_ids = route.edge_ids;
  msg.length_m = route.length_m;
  msg.estimated_time_s = route.estimated_time_s;
  msg.status = route.success ? "ok" : route.message;
  return msg;
}

low_speed_av_interfaces::msg::Trajectory PlanningNode::to_msg(
  const Trajectory & trajectory, const std::string & status) const
{
  low_speed_av_interfaces::msg::Trajectory msg;
  msg.header.stamp = now();
  msg.trajectory_id = "trajectory_" + std::to_string(now().nanoseconds());
  msg.source_package_id = package_ ? package_->package_id : "";
  msg.planner_algorithm = global_planner_algorithm_ + "/" + motion_planner_algorithm_ + "/" + speed_planner_algorithm_;
  msg.emergency_stop = false;
  msg.status = status;
  double relative_time = 0.0;
  for (std::size_t i = 0; i < trajectory.size(); ++i) {
    const auto & wp = trajectory[i];
    low_speed_av_interfaces::msg::TrajectoryPoint point;
    point.header = msg.header;
    point.index = static_cast<uint32_t>(i);
    point.waypoint_id = wp.waypoint_id;
    point.edge_id = wp.edge_id;
    point.path_id = wp.path_id;
    point.x_m = wp.x_m;
    point.y_m = wp.y_m;
    point.yaw_rad = wp.yaw_rad;
    point.kappa_1pm = wp.kappa_1pm;
    point.s_m = wp.route_s_m;
    point.v_mps = wp.target_speed_mps;
    point.a_mps2 = 0.0;
    point.relative_time_s = relative_time;
    point.gear = static_cast<int8_t>(wp.gear);
    point.behavior = wp.behavior;
    msg.points.push_back(point);
    relative_time += wp.target_speed_mps > 0.05 && i + 1 < trajectory.size() ?
      (trajectory[i + 1].route_s_m - wp.route_s_m) / wp.target_speed_mps : 0.1;
  }
  return msg;
}

void PlanningNode::on_reload_roadnet(
  const std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Request> request,
  std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Response> response)
{
  try {
    RoadnetLoader::Options options;
    options.reject_failed_validation = get_parameter("roadnet.reject_failed_validation").as_bool();
    options.verify_checksums = get_parameter("roadnet.verify_checksums").as_bool();
    package_ = std::make_shared<RoadnetPackage>(loader_.load(request->package_path, options));
    clear_cached_plan();
    response->success = true;
    response->package_id = package_->package_id;
    response->message = "roadnet ready";
    publish_roadnet_status(true, response->message);
    publish_status("active", response->message);
  } catch (const std::exception & e) {
    package_.reset();
    clear_cached_plan();
    response->success = false;
    response->message = e.what();
    publish_roadnet_status(false, response->message);
    publish_status("failure", response->message);
  }
}

void PlanningNode::on_plan_route(
  const std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Request> request,
  std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Response> response)
{
  std::string start_diagnostic;
  std::string goal_diagnostic;
    const auto start_anchor = resolve_start_anchor(
    request->start_node_id, request->start_task_point_id, &start_diagnostic);
  const auto goal_anchor = resolve_goal_anchor(
    request->goal_node_id, request->goal_task_point_id, request->goal_parking_point_id, &goal_diagnostic);
  if (!start_anchor || !goal_anchor || start_anchor->node_id.empty() || goal_anchor->node_id.empty()) {
    response->success = false;
    response->message = compose_resolution_failure_message(
      !start_anchor || start_anchor->node_id.empty(),
      !goal_anchor || goal_anchor->node_id.empty(),
      start_diagnostic,
      goal_diagnostic);
    publish_status("failure", response->message);
    publish_failure_trajectory(response->message);
    return;
  }

  try {
    const auto decision = plan_route_between_anchors(*start_anchor, *goal_anchor);
    const auto & route = decision.route;
    const auto route_for_message = route_with_goal_edge_for_message(route, *goal_anchor);
    response->route = to_msg(route_for_message);
    if (!route.success) {
      global_route_pub_->publish(response->route);
      has_last_route_msg_ = false;
      response->success = false;
      response->message = "route planning failed: " + route.message +
        (decision.note.empty() ? "" : "; " + decision.note) +
        (start_diagnostic.empty() ? "" : "; start: " + start_diagnostic) +
        (goal_diagnostic.empty() ? "" : "; goal: " + goal_diagnostic);
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    const auto trajectory = compute_trajectory_to_goal_anchor(route, *start_anchor, *goal_anchor);
    if (trajectory.empty()) {
      response->success = false;
      response->message = "motion planner produced empty trajectory";
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    auto summarized_route = route_for_message;
    recompute_route_summary(summarized_route, last_full_reference_path_);
    response->route = to_msg(summarized_route);
    global_route_pub_->publish(response->route);
    if (start_anchor->type == RoadnetAnchor::Type::CurrentPose && latest_pose_) {
      const double max_first_distance =
        get_parameter("planning.start_anchor.max_first_trajectory_point_distance_m").as_double();
      const double first_distance = distance_xy(
        latest_pose_->x_m, latest_pose_->y_m, trajectory.front().x_m, trajectory.front().y_m);
      if (max_first_distance > 0.0 && first_distance > max_first_distance) {
        response->success = false;
        std::ostringstream oss;
        oss << "local trajectory start is too far from current pose: distance=" << first_distance <<
          "m max=" << max_first_distance << "m";
        response->message = oss.str();
        publish_status("failure", response->message);
        publish_failure_trajectory(response->message);
        return;
      }
    }
    std::string continuity_error;
    if (!trajectory_is_continuous(trajectory, &continuity_error)) {
      response->success = false;
      response->message = "local trajectory continuity check failed: " + continuity_error;
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    last_route_msg_ = response->route;
    has_last_route_msg_ = true;
    last_trajectory_msg_ = to_msg(trajectory, "ok");
    has_last_trajectory_msg_ = true;
    trajectory_pub_->publish(last_trajectory_msg_);
    response->success = true;
    response->message = compose_success_message(start_diagnostic, goal_diagnostic);
    if (!decision.note.empty()) {
      response->message += "; " + decision.note;
    }
    const auto diagnostics = start_diagnostic + (goal_diagnostic.empty() ? "" : "; " + goal_diagnostic);
    const auto status_message = diagnostics.empty() && decision.note.empty() ?
      "planned route with " + std::to_string(trajectory.size()) + " trajectory points" :
      "planned route with " + std::to_string(trajectory.size()) + " trajectory points" +
        (decision.note.empty() ? "" : "; " + decision.note) +
        (diagnostics.empty() ? "" : "; " + diagnostics);
    publish_status("active", status_message);
  } catch (const std::exception & e) {
    response->success = false;
    response->message = e.what();
    publish_status("failure", response->message);
    publish_failure_trajectory(response->message);
  }
}

void PlanningNode::on_plan_mission(
  const std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Request> request,
  std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Response> response)
{
  auto normalized = [](std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    return value;
  };

  std::string start_diagnostic;
  std::string goal_diagnostic;
  const auto start_type = normalized(request->start_type);
  const auto goal_type = normalized(request->goal_type);

  std::optional<RoadnetAnchor> start_anchor;
  if (start_type.empty() || any_of_type(start_type, {"current_pose", "current", "current_position"})) {
    start_anchor = resolve_start_anchor("", "", &start_diagnostic);
  } else if (any_of_type(start_type, {"node", "node_id"})) {
    start_anchor = resolve_start_anchor(request->start_id, "", &start_diagnostic);
  } else if (any_of_type(start_type, {"task", "task_point"})) {
    start_anchor = resolve_start_anchor("", request->start_id, &start_diagnostic);
  } else if (any_of_type(start_type, {"parking", "parking_point", "park"}) && package_) {
    start_anchor = make_semantic_anchor(
      request->start_id, package_->parking_points, "parking point", RoadnetAnchor::Type::ParkingPoint, false,
      &start_diagnostic);
  } else if (any_of_type(start_type, {"charging", "charging_point", "charge"}) && package_) {
    start_anchor = make_semantic_anchor(
      request->start_id, package_->charging_points, "charging point", RoadnetAnchor::Type::ChargingPoint, false,
      &start_diagnostic);
  } else {
    start_diagnostic = "unsupported mission start_type: " + request->start_type;
  }

  std::optional<RoadnetAnchor> goal_anchor;
  if (any_of_type(goal_type, {"node", "node_id"})) {
    goal_anchor = resolve_goal_anchor(request->goal_id, "", "", &goal_diagnostic);
  } else if (any_of_type(goal_type, {"task", "task_point"})) {
    goal_anchor = resolve_goal_anchor("", request->goal_id, "", &goal_diagnostic);
  } else if (any_of_type(goal_type, {"parking", "parking_point", "park"})) {
    goal_anchor = resolve_goal_anchor("", "", request->goal_id, &goal_diagnostic);
  } else if (any_of_type(goal_type, {"charging", "charging_point", "charge"}) && package_) {
    goal_anchor = make_semantic_anchor(
      request->goal_id, package_->charging_points, "charging point", RoadnetAnchor::Type::ChargingPoint, true,
      &goal_diagnostic);
  } else {
    goal_diagnostic = "unsupported mission goal_type: " + request->goal_type;
  }

  if (!start_anchor || !goal_anchor || start_anchor->node_id.empty() || goal_anchor->node_id.empty()) {
    response->success = false;
    response->message = compose_resolution_failure_message(
      !start_anchor || start_anchor->node_id.empty(),
      !goal_anchor || goal_anchor->node_id.empty(),
      start_diagnostic,
      goal_diagnostic);
    publish_status("failure", response->message);
    publish_failure_trajectory(response->message);
    return;
  }

  try {
    const auto decision = plan_route_between_anchors(*start_anchor, *goal_anchor);
    const auto & route = decision.route;
    const auto route_for_message = route_with_goal_edge_for_message(route, *goal_anchor);
    response->route = to_msg(route_for_message);
    if (!route.success) {
      global_route_pub_->publish(response->route);
      has_last_route_msg_ = false;
      response->success = false;
      response->message = "route planning failed: " + route.message +
        (decision.note.empty() ? "" : "; " + decision.note) +
        (start_diagnostic.empty() ? "" : "; start: " + start_diagnostic) +
        (goal_diagnostic.empty() ? "" : "; goal: " + goal_diagnostic);
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    const auto trajectory = compute_trajectory_to_goal_anchor(route, *start_anchor, *goal_anchor);
    if (trajectory.empty()) {
      response->success = false;
      response->message = "motion planner produced empty trajectory";
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    auto summarized_route = route_for_message;
    recompute_route_summary(summarized_route, last_full_reference_path_);
    response->route = to_msg(summarized_route);
    global_route_pub_->publish(response->route);
    if (start_anchor->type == RoadnetAnchor::Type::CurrentPose && latest_pose_) {
      const double max_first_distance =
        get_parameter("planning.start_anchor.max_first_trajectory_point_distance_m").as_double();
      const double first_distance = distance_xy(
        latest_pose_->x_m, latest_pose_->y_m, trajectory.front().x_m, trajectory.front().y_m);
      if (max_first_distance > 0.0 && first_distance > max_first_distance) {
        response->success = false;
        std::ostringstream oss;
        oss << "local trajectory start is too far from current pose: distance=" << first_distance <<
          "m max=" << max_first_distance << "m";
        response->message = oss.str();
        publish_status("failure", response->message);
        publish_failure_trajectory(response->message);
        return;
      }
    }
    std::string continuity_error;
    if (!trajectory_is_continuous(trajectory, &continuity_error)) {
      response->success = false;
      response->message = "local trajectory continuity check failed: " + continuity_error;
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    last_route_msg_ = response->route;
    has_last_route_msg_ = true;
    last_trajectory_msg_ = to_msg(trajectory, "ok");
    has_last_trajectory_msg_ = true;
    trajectory_pub_->publish(last_trajectory_msg_);
    response->success = true;
    response->message = compose_success_message(start_diagnostic, goal_diagnostic);
    if (!decision.note.empty()) {
      response->message += "; " + decision.note;
    }
    publish_status(
      "active",
      "planned mission with " + std::to_string(trajectory.size()) +
        " trajectory points" +
        (decision.note.empty() ? "" : "; " + decision.note) +
        (start_diagnostic.empty() ? "" : "; " + start_diagnostic) +
        (goal_diagnostic.empty() ? "" : "; " + goal_diagnostic));
  } catch (const std::exception & e) {
    response->success = false;
    response->message = e.what();
    publish_status("failure", response->message);
    publish_failure_trajectory(response->message);
  }
}

void PlanningNode::on_set_planner_algorithm(
  const std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Request> request,
  std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Response> response)
{
  try {
    if (!request->global_planner_algorithm.empty()) {
      (void)GlobalPlannerFactory::create(request->global_planner_algorithm);
      global_planner_algorithm_ = request->global_planner_algorithm;
    }
    if (!request->motion_planner_algorithm.empty()) {
      (void)MotionPlannerFactory::create(request->motion_planner_algorithm);
      motion_planner_algorithm_ = request->motion_planner_algorithm;
    }
    if (!request->speed_planner_algorithm.empty()) {
      (void)SpeedPlannerFactory::create(request->speed_planner_algorithm);
      speed_planner_algorithm_ = request->speed_planner_algorithm;
    }
    progress_tracker_.reset();
    response->success = true;
    response->message = "planner algorithms updated";
    publish_status("active", response->message);
  } catch (const std::exception & e) {
    response->success = false;
    response->message = e.what();
    publish_status("failure", response->message);
  }
}

}  // namespace low_speed_av_planning
