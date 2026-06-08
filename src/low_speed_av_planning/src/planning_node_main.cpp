#include <rclcpp/rclcpp.hpp>

#include "low_speed_av_planning/planning_node.hpp"

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<low_speed_av_planning::PlanningNode>());
  rclcpp::shutdown();
  return 0;
}
