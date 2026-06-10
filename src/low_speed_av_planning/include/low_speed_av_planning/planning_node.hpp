#pragma once

#include <memory>
#include <optional>
#include <string>

#include <geometry_msgs/msg/pose_stamped.hpp>
#include <rclcpp/rclcpp.hpp>

#include <low_speed_av_interfaces/msg/global_route.hpp>
#include <low_speed_av_interfaces/msg/module_status.hpp>
#include <low_speed_av_interfaces/msg/roadnet_status.hpp>
#include <low_speed_av_interfaces/msg/trajectory.hpp>
#include <low_speed_av_interfaces/srv/plan_route.hpp>
#include <low_speed_av_interfaces/srv/reload_roadnet.hpp>
#include <low_speed_av_interfaces/srv/set_planner_algorithm.hpp>

#include "low_speed_av_planning/global_planner_base.hpp"
#include "low_speed_av_planning/motion_planner_base.hpp"
#include "low_speed_av_planning/roadnet_loader.hpp"
#include "low_speed_av_planning/speed_planner_base.hpp"

namespace low_speed_av_planning {

class PlanningNode : public rclcpp::Node {
public:
  explicit PlanningNode(const rclcpp::NodeOptions & options = rclcpp::NodeOptions());

private:
  void load_package_from_parameter();
  void publish_status(const std::string & state, const std::string & message);
  void publish_roadnet_status(bool ready, const std::string & message);
  void publish_failure_trajectory(const std::string & reason);
  PlanResult compute_route(const std::string & start_node_id, const std::string & goal_node_id);
  Trajectory compute_trajectory(const PlanResult & route);
  void apply_semantic_speed_limits(Trajectory & trajectory) const;
  std::string resolve_start_node(
    const std::string & node_id,
    const std::string & task_point_id,
    std::string * diagnostic) const;
  std::string resolve_goal_node(
    const std::string & node_id,
    const std::string & task_point_id,
    const std::string & parking_point_id) const;
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
  rclcpp::Publisher<low_speed_av_interfaces::msg::ModuleStatus>::SharedPtr planning_status_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::RoadnetStatus>::SharedPtr roadnet_status_pub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Service<low_speed_av_interfaces::srv::ReloadRoadnet>::SharedPtr reload_srv_;
  rclcpp::Service<low_speed_av_interfaces::srv::PlanRoute>::SharedPtr plan_route_srv_;
  rclcpp::Service<low_speed_av_interfaces::srv::SetPlannerAlgorithm>::SharedPtr set_algorithm_srv_;
  std::optional<Pose2d> latest_pose_;
  rclcpp::Time latest_pose_receive_time_;
};

}  // namespace low_speed_av_planning
