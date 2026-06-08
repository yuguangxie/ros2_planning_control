#include "low_speed_av_planning/planning_node.hpp"

#include <algorithm>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "low_speed_av_planning/global_planner_factory.hpp"
#include "low_speed_av_planning/motion_planner_base.hpp"
#include "low_speed_av_planning/speed_planner_base.hpp"
#include "low_speed_av_planning/topology_graph.hpp"

namespace low_speed_av_planning {
namespace {

bool point_in_polygon(const Pose2d & point, const std::vector<Pose2d> & polygon)
{
  if (polygon.size() < 3U) {
    return false;
  }
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

bool is_speed_zone(const SemanticArea & area)
{
  return area.type == "speed_zone" || area.speed_limit_mps > 0.0;
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
  declare_parameter<std::string>("global_planner.algorithm", "astar");
  declare_parameter<double>("global_planner.heuristic_weight", 1.0);
  declare_parameter<bool>("global_planner.allow_reverse", true);
  declare_parameter<std::vector<std::string>>("global_planner.blocked_edges", {});
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

  global_planner_algorithm_ = get_parameter("global_planner.algorithm").as_string();
  motion_planner_algorithm_ = get_parameter("motion_planner.algorithm").as_string();
  speed_planner_algorithm_ = get_parameter("speed_planner.algorithm").as_string();

  global_route_pub_ = create_publisher<low_speed_av_interfaces::msg::GlobalRoute>(
    get_parameter("topics.global_route_topic").as_string(), 10);
  trajectory_pub_ = create_publisher<low_speed_av_interfaces::msg::Trajectory>(
    get_parameter("topics.trajectory_topic").as_string(), 10);
  planning_status_pub_ = create_publisher<low_speed_av_interfaces::msg::ModuleStatus>(
    get_parameter("topics.planning_status_topic").as_string(), 10);
  roadnet_status_pub_ = create_publisher<low_speed_av_interfaces::msg::RoadnetStatus>(
    get_parameter("topics.roadnet_status_topic").as_string(), 10);

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
  set_algorithm_srv_ = create_service<low_speed_av_interfaces::srv::SetPlannerAlgorithm>(
    "~/set_planner_algorithm",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Response> response) {
      on_set_planner_algorithm(request, response);
    });

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
    RCLCPP_INFO(get_logger(), "loaded AD package %s", package_->package_id.c_str());
    publish_roadnet_status(true, "roadnet ready");
    publish_status("active", "roadnet ready");
  } catch (const std::exception & e) {
    package_.reset();
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
  roadnet_status_pub_->publish(status);
}

void PlanningNode::publish_failure_trajectory(const std::string & reason)
{
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
  trajectory_pub_->publish(msg);
}

GlobalPlannerOptions PlanningNode::read_global_options() const
{
  GlobalPlannerOptions options;
  options.allow_reverse = get_parameter("global_planner.allow_reverse").as_bool();
  options.heuristic_weight = get_parameter("global_planner.heuristic_weight").as_double();
  for (const auto & edge_id : get_parameter("global_planner.blocked_edges").as_string_array()) {
    options.blocked_edges.insert(edge_id);
  }
  if (package_) {
    options.blocked_edges.insert(package_->blocked_edges.begin(), package_->blocked_edges.end());
  }
  return options;
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

void PlanningNode::apply_semantic_speed_limits(Trajectory & trajectory) const
{
  if (!package_) {
    return;
  }
  for (auto & wp : trajectory) {
    for (const auto & area : package_->areas) {
      if (!is_speed_zone(area) || area.speed_limit_mps <= 0.0) {
        continue;
      }
      if (point_in_polygon({wp.x_m, wp.y_m, wp.yaw_rad}, area.polygon)) {
        wp.target_speed_mps = std::min(wp.target_speed_mps, area.speed_limit_mps);
        if (area.type == "speed_zone") {
          wp.behavior = "semantic_speed_zone:" + area.id;
        }
      }
    }
  }
}

std::string PlanningNode::resolve_goal_node(
  const std::string & node_id,
  const std::string & task_point_id,
  const std::string & parking_point_id) const
{
  if (!node_id.empty()) {
    return node_id;
  }
  if (package_ && !task_point_id.empty()) {
    const auto it = package_->task_points.find(task_point_id);
    if (it != package_->task_points.end() && !it->second.linked_node_id.empty()) {
      return it->second.linked_node_id;
    }
    if (it != package_->task_points.end() && !it->second.linked_edge_id.empty()) {
      const auto edge_it = std::find_if(package_->edges.begin(), package_->edges.end(), [&](const auto & edge) {
        return edge.id == it->second.linked_edge_id;
      });
      if (edge_it != package_->edges.end()) {
        return edge_it->to_node_id;
      }
    }
  }
  if (package_ && !parking_point_id.empty()) {
    const auto it = package_->parking_points.find(parking_point_id);
    if (it != package_->parking_points.end() && !it->second.linked_node_id.empty()) {
      return it->second.linked_node_id;
    }
    if (it != package_->parking_points.end() && !it->second.linked_edge_id.empty()) {
      const auto edge_it = std::find_if(package_->edges.begin(), package_->edges.end(), [&](const auto & edge) {
        return edge.id == it->second.linked_edge_id;
      });
      if (edge_it != package_->edges.end()) {
        return edge_it->to_node_id;
      }
    }
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
    response->success = true;
    response->package_id = package_->package_id;
    response->message = "roadnet ready";
    publish_roadnet_status(true, response->message);
    publish_status("active", response->message);
  } catch (const std::exception & e) {
    package_.reset();
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
  const auto start = resolve_goal_node(
    request->start_node_id, request->start_task_point_id, "");
  const auto goal = resolve_goal_node(
    request->goal_node_id, request->goal_task_point_id, request->goal_parking_point_id);
  if (start.empty() || goal.empty()) {
    response->success = false;
    response->message = "start or goal node cannot be resolved";
    publish_status("failure", response->message);
    publish_failure_trajectory(response->message);
    return;
  }

  try {
    const auto route = compute_route(start, goal);
    response->route = to_msg(route);
    global_route_pub_->publish(response->route);
    if (!route.success) {
      response->success = false;
      response->message = route.message;
      publish_status("failure", route.message);
      publish_failure_trajectory(route.message);
      return;
    }
    const auto trajectory = compute_trajectory(route);
    if (trajectory.empty()) {
      response->success = false;
      response->message = "motion planner produced empty trajectory";
      publish_status("failure", response->message);
      publish_failure_trajectory(response->message);
      return;
    }
    trajectory_pub_->publish(to_msg(trajectory, "ok"));
    response->success = true;
    response->message = "ok";
    publish_status("active", "planned route with " + std::to_string(trajectory.size()) + " trajectory points");
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
