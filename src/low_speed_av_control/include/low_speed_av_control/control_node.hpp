#pragma once

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include <chassis_interfaces/msg/scu_control_command.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <low_speed_av_interfaces/msg/control_command.hpp>
#include <low_speed_av_interfaces/msg/module_status.hpp>
#include <low_speed_av_interfaces/msg/trajectory.hpp>
#include <low_speed_av_interfaces/msg/vehicle_state.hpp>
#include <low_speed_av_interfaces/srv/set_controller_algorithm.hpp>
#include <std_srvs/srv/trigger.hpp>

#include "low_speed_av_control/command_limiter.hpp"
#include "low_speed_av_control/command_smoother.hpp"
#include "low_speed_av_control/control_runtime_helpers.hpp"
#include "low_speed_av_control/controller_factory.hpp"
#include "low_speed_av_control/safety_state_machine.hpp"
#include "low_speed_av_control/scu_command_mapper.hpp"
#include "low_speed_av_control/vehicle_model_factory.hpp"

namespace low_speed_av_control {

class ControlNode : public rclcpp::Node {
public:
  explicit ControlNode(
      const rclcpp::NodeOptions &options = rclcpp::NodeOptions());

private:
  // Create a safe stop command for timeout, empty trajectory and future estop.
  ControlCommand controlled_stop(const std::string &reason,
                                 bool emergency_stop) const;
  // ROS subscriptions update the latest inputs used by the periodic control
  // loop.
  void on_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg);
  void
  on_trajectory(const low_speed_av_interfaces::msg::Trajectory::SharedPtr msg);
  void on_vehicle_state(
      const low_speed_av_interfaces::msg::VehicleState::SharedPtr msg);
  void on_safety_status(
      const low_speed_av_interfaces::msg::ModuleStatus::SharedPtr msg);
  void on_timer();
  ControlCommand compute_tracking_command();
  ControlCommand finalize_command(const ControlCommand &command);
  void publish_status(const std::string &state, uint8_t level,
                      const std::string &message);
  void publish_periodic_status(const std::string &state, uint8_t level,
                               const std::string &message);
  std::string diagnostic_message(const std::string &reason) const;
  void load_runtime_options();
  SafetyInputs make_safety_inputs() const;
  bool inputs_ready_for_estop_clear(std::string *reason) const;
  void
  on_clear_estop(const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
                 std::shared_ptr<std_srvs::srv::Trigger::Response> response);
  void on_set_controller_algorithm(
      const std::shared_ptr<
          low_speed_av_interfaces::srv::SetControllerAlgorithm::Request>
          request,
      std::shared_ptr<
          low_speed_av_interfaces::srv::SetControllerAlgorithm::Response>
          response);
  // Convert the internal command type to the ROS2 interface message.
  void publish_command(const ControlCommand &command);
  bool publish_internal_command() const;
  bool publish_scu_command() const;
  CommandLimiter limiter_;
  CommandSmoother smoother_;
  ScuCommandMapper scu_mapper_;
  std::unique_ptr<ControllerBase> controller_;
  std::unique_ptr<VehicleModelBase> vehicle_model_;
  ControllerOptions controller_options_;
  VehicleLimits vehicle_limits_;
  SmootherOptions smoother_options_;
  ScuCommandOptions scu_options_;
  ControlTimingOptions timing_options_;
  TrackingProgressOptions progress_options_;
  TrackingProgressTracker progress_tracker_;
  ControlCadenceMonitor cadence_monitor_;
  std::string output_mode_{"both"};
  std::string controller_algorithm_{"pure_pursuit"};
  std::string vehicle_model_name_{"front_ackermann"};
  Pose2d pose_;
  VehicleState state_;
  Trajectory trajectory_;
  ControlSafetyStateMachine safety_state_machine_;
  ControlSafetyState safety_state_{ControlSafetyState::WaitInputs};
  SafetyEstopLatch safety_estop_latch_;
  bool safety_request_active_{false};
  bool safety_estop_latched_{true};
  double safety_clear_speed_threshold_mps_{0.05};
  bool force_ready_cycle_{false};
  bool tracking_was_active_{false};
  bool limiter_saturated_{false};
  double current_control_dt_s_{0.02};
  bool have_pose_{false};
  bool pose_valid_{false};
  bool have_trajectory_{false};
  bool trajectory_valid_{false};
  bool trajectory_emergency_active_{false};
  std::string trajectory_id_;
  std::string trajectory_source_package_id_;
  std::string trajectory_status_;
  std::string trajectory_invalid_reason_;
  std::vector<std::string> allowed_trajectory_statuses_{"ok"};
  double trajectory_s_tolerance_m_{1.0e-4};
  bool vehicle_state_required_{false};
  bool have_vehicle_state_{false};
  bool vehicle_state_valid_{true};
  double vehicle_state_timeout_s_{0.5};
  std::chrono::steady_clock::time_point last_pose_receive_time_{};
  std::chrono::steady_clock::time_point last_trajectory_receive_time_{};
  std::chrono::steady_clock::time_point last_vehicle_state_receive_time_{};
  rclcpp::Time last_status_time_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::Trajectory>::SharedPtr
      trajectory_sub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::VehicleState>::SharedPtr
      vehicle_state_sub_;
  rclcpp::Subscription<low_speed_av_interfaces::msg::ModuleStatus>::SharedPtr
      safety_status_sub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::ControlCommand>::SharedPtr
      command_pub_;
  rclcpp::Publisher<chassis_interfaces::msg::ScuControlCommand>::SharedPtr
      scu_command_pub_;
  rclcpp::Publisher<low_speed_av_interfaces::msg::ModuleStatus>::SharedPtr
      status_pub_;
  rclcpp::Service<low_speed_av_interfaces::srv::SetControllerAlgorithm>::
      SharedPtr set_algorithm_srv_;
  rclcpp::Service<std_srvs::srv::Trigger>::SharedPtr clear_estop_srv_;
  rclcpp::TimerBase::SharedPtr timer_;
};

} // namespace low_speed_av_control
