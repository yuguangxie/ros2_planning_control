#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <rclcpp/rclcpp.hpp>

#include <low_speed_av_interfaces/msg/global_route.hpp>
#include <low_speed_av_interfaces/msg/module_status.hpp>
#include <low_speed_av_interfaces/msg/roadnet_status.hpp>
#include <low_speed_av_interfaces/msg/trajectory.hpp>
#include <low_speed_av_interfaces/srv/plan_mission.hpp>
#include <low_speed_av_interfaces/srv/plan_route.hpp>
#include <low_speed_av_interfaces/srv/reload_roadnet.hpp>
#include <low_speed_av_interfaces/srv/set_planner_algorithm.hpp>

#include "low_speed_av_planning/global_planner_base.hpp"
#include "low_speed_av_planning/motion_planner_base.hpp"
#include "low_speed_av_planning/roadnet_loader.hpp"
#include "low_speed_av_planning/speed_planner_base.hpp"

namespace low_speed_av_planning {

struct RoadnetAnchor {
  enum class Type {
    CurrentPose,
    Node,
    EdgePoint,
    TaskPoint,
    ParkingPoint,
    ChargingPoint
  };

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

struct RouteDecision {
  PlanResult route;
  std::string note;
};

class PlanningNode : public rclcpp::Node {
public:
  explicit PlanningNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void load_package_from_parameter();
  void publish_status(const std::string & state, const std::string & message);
  void publish_roadnet_status(bool ready, const std::string & message);
  void republish_last_route();
  void republish_last_trajectory();
  void republish_last_full_reference_path();
  void republish_last_roadnet_status();
  void clear_cached_plan();
  void publish_failure_trajectory(const std::string & reason);
  PlanResult compute_route(const std::string & start_node_id, const std::string & goal_node_id);
  Trajectory compute_trajectory(const PlanResult & route);
  Trajectory compute_trajectory_to_goal_anchor(
    const PlanResult & route,
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor);
  Trajectory compute_full_reference_path_to_goal_anchor(
    const PlanResult & route,
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor);
  Trajectory make_local_trajectory_from_full_reference(const Trajectory & full_reference) const;
  bool trajectory_is_continuous(const Trajectory & trajectory, std::string * diagnostic) const;
  Trajectory make_arrived_stop_trajectory(const RoadnetAnchor & anchor, const std::string & behavior) const;
  Trajectory make_edge_segment_trajectory(
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor,
    bool reverse) const;
  void append_edge_segment(
    Trajectory & trajectory,
    const RoadnetAnchor & goal_anchor,
    bool reverse) const;
  void append_edge_segment_between(
    Trajectory & trajectory,
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor,
    bool reverse) const;
  void append_start_edge_remaining_segment(Trajectory & trajectory, const RoadnetAnchor & start_anchor) const;
  void append_waypoint(Trajectory & trajectory, Waypoint waypoint) const;
  void regenerate_route_s(Trajectory & trajectory) const;
  void apply_semantic_speed_limits(Trajectory & trajectory) const;
  bool has_arrived(const RoadnetAnchor & goal_anchor) const;
  bool reverse_planning_allowed() const;
  bool reverse_local_segment_allowed() const;
  bool start_and_goal_on_same_edge(const RoadnetAnchor & start_anchor, const RoadnetAnchor & goal_anchor) const;
  bool goal_is_behind_start_on_same_edge(
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor) const;
  RouteDecision plan_route_between_anchors(
    const RoadnetAnchor & start_anchor,
    const RoadnetAnchor & goal_anchor);
  std::optional<RoadnetAnchor> resolve_start_anchor(
    const std::string & node_id,
    const std::string & task_point_id,
    std::string * diagnostic) const;
  std::optional<RoadnetAnchor> resolve_goal_anchor(
    const std::string & node_id,
    const std::string & task_point_id,
    const std::string & parking_point_id,
    std::string * diagnostic) const;
  std::optional<RoadnetAnchor> make_node_anchor(
    const std::string & node_id,
    RoadnetAnchor::Type type,
    const std::string & point_id,
    std::string * diagnostic) const;
  std::optional<RoadnetAnchor> make_semantic_anchor(
    const std::string & point_id,
    const std::map<std::string, SemanticPoint> & points,
    const std::string & point_kind,
    RoadnetAnchor::Type type,
    bool prefer_edge_to_node,
    std::string * diagnostic) const;
  std::optional<RoadnetAnchor> match_current_pose_to_start_anchor(std::string * diagnostic) const;
  bool project_anchor_to_edge(RoadnetAnchor & anchor, std::string * diagnostic) const;
  std::string resolve_start_node(
    const std::string & node_id,
    const std::string & task_point_id,
    std::string * diagnostic) const;
  std::string resolve_goal_node(
    const std::string & node_id,
    const std::string & task_point_id,
    const std::string & parking_point_id,
    std::string * diagnostic) const;
  std::string resolve_semantic_point_node(
    const std::string & point_id,
    const std::map<std::string, SemanticPoint> & points,
    const std::string & point_kind,
    bool prefer_edge_to_node,
    std::string * diagnostic) const;
  std::optional<std::string> match_current_pose_to_start_node(std::string * diagnostic) const;
  void on_localization_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg);
  low_speed_av_interfaces::msg::GlobalRoute to_msg(const PlanResult & route) const;
  low_speed_av_interfaces::msg::Trajectory to_msg(const Trajectory & trajectory, const std::string & status) const;
  GlobalPlannerOptions read_global_options() const;
  MotionPlannerOptions read_motion_options() const;
  SpeedPlannerOptions read_speed_options() const;
  void on_reload_roadnet(
    const std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Request> request,
    std::shared_ptr<low_speed_av_interfaces::srv::ReloadRoadnet::Response> response);
  void on_plan_route(
    const std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Request> request,
    std::shared_ptr<low_speed_av_interfaces::srv::PlanRoute::Response> response);
  void on_plan_mission(
    const std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Request> request,
    std::shared_ptr<low_speed_av_interfaces::srv::PlanMission::Response> response);
  void on_set_planner_algorithm(
    const std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Request> request,
    std::shared_ptr<low_speed_av_interfaces::srv::SetPlannerAlgorithm::Response> response);

  RoadnetLoader loader_;
  std::shared_ptr<RoadnetPackage> package_;
  std::string global_planner_algorithm_{"astar"};
  std::string motion_planner_algorithm_{"reference_line"};
  std::string speed_planner_algorithm_{"curvature"};
  rclcpp::Publisher<low_speed_av_interfaces::msg::GlobalRoute>::SharedPtr global_route_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::Trajectory>::SharedPtr trajectory_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::Trajectory>::SharedPtr full_reference_path_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::ModuleStatus>::SharedPtr planning_status_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::RoadnetStatus>::SharedPtr roadnet_status_pub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Service<low_speed_av_interfaces::srv::ReloadRoadnet>::SharedPtr reload_srv_;
  rclcpp::Service<low_speed_av_interfaces::srv::PlanRoute>::SharedPtr plan_route_srv_;
  rclcpp::Service<low_speed_av_interfaces::srv::PlanMission>::SharedPtr plan_mission_srv_;
  rclcpp::Service<low_speed_av_interfaces::srv::SetPlannerAlgorithm>::SharedPtr set_algorithm_srv_;
  rclcpp::TimerBase::SharedPtr route_republish_timer_;
  rclcpp::TimerBase::SharedPtr trajectory_republish_timer_;
  rclcpp::TimerBase::SharedPtr roadnet_status_timer_;
  std::optional<Pose2d> latest_pose_;
  rclcpp::Time latest_pose_receive_time_;
  low_speed_av_interfaces::msg::GlobalRoute last_route_msg_;
  low_speed_av_interfaces::msg::Trajectory last_trajectory_msg_;
  low_speed_av_interfaces::msg::Trajectory last_full_reference_path_msg_;
  low_speed_av_interfaces::msg::RoadnetStatus last_roadnet_status_msg_;
  Trajectory last_full_reference_path_;
  bool has_last_route_msg_{false};
  bool has_last_trajectory_msg_{false};
  bool has_last_full_reference_path_msg_{false};
  bool has_last_roadnet_status_msg_{false};
};

}  // namespace low_speed_av_planning
