#include "low_speed_av_control/control_node.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace low_speed_av_control {

ControlNode::ControlNode(const rclcpp::NodeOptions & options)
: rclcpp::Node("low_speed_av_control", options)
{
  declare_parameter<std::string>("topics.localization_pose_topic", "/localization/pose");
  declare_parameter<std::string>("topics.trajectory_topic", "/planning/trajectory");
  declare_parameter<std::string>("topics.vehicle_state_topic", "/vehicle/state");
  declare_parameter<std::string>("topics.safety_status_topic", "/safety/status");
  declare_parameter<std::string>("topics.control_command_topic", "/control/command");
  declare_parameter<std::string>("topics.scu_command_topic", "/yunle_chassis/control/scu_control_command");
  declare_parameter<std::string>("topics.control_status_topic", "/control/status");
  declare_parameter<std::string>("output.mode", "scu_control_command");
  declare_parameter<bool>("safety.estop_latched", true);
  declare_parameter<int>("safety.clear_level", 0);
  declare_parameter<std::string>("safety.clear_state", "ok");
  declare_parameter<std::string>("controller.algorithm", "pure_pursuit");
  declare_parameter<double>("controller.control_rate_hz", 50.0);
  declare_parameter<double>("controller.localization_timeout_s", 0.2);
  declare_parameter<double>("controller.trajectory_timeout_s", 0.5);
  declare_parameter<std::string>("vehicle.model", "front_ackermann");
  declare_parameter<double>("vehicle.wheel_base_m", 1.2);
  declare_parameter<double>("vehicle.max_speed_mps", 1.2);
  declare_parameter<double>("vehicle.max_accel_mps2", 0.5);
  declare_parameter<double>("vehicle.max_decel_mps2", 0.8);
  declare_parameter<double>("vehicle.max_front_steer_rad", 0.6);
  declare_parameter<double>("vehicle.max_rear_steer_rad", 0.6);
  declare_parameter<double>("vehicle.max_front_steer_rate_radps", 0.5);
  declare_parameter<double>("vehicle.max_rear_steer_rate_radps", 0.5);
  declare_parameter<double>("vehicle.rear_steer_ratio", 0.5);
  declare_parameter<double>("pure_pursuit.lookahead_min_m", 0.8);
  declare_parameter<double>("pure_pursuit.lookahead_max_m", 3.0);
  declare_parameter<double>("pure_pursuit.lookahead_speed_gain", 1.2);
  declare_parameter<double>("stanley.k", 0.8);
  declare_parameter<double>("stanley.epsilon_mps", 0.1);
  declare_parameter<double>("stanley.max_correction_rad", 0.5);
  declare_parameter<double>("lqr.q_lateral_error", 3.0);
  declare_parameter<double>("lqr.q_heading_error", 2.0);
  declare_parameter<double>("lqr.r_steering", 1.0);
  declare_parameter<int>("lqr.max_iterations", 80);
  declare_parameter<double>("lqr.convergence_eps", 1.0e-6);
  declare_parameter<double>("lqr.min_speed_mps", 0.2);
  declare_parameter<double>("lqr.preview_time_s", 0.2);
  declare_parameter<bool>("lqr.use_curvature_feedforward", true);
  declare_parameter<double>("lqr.max_steering_angle_rad", 0.52);
  declare_parameter<int>("mpc_sampler.horizon_steps", 10);
  declare_parameter<double>("mpc_sampler.dt_s", 0.1);
  declare_parameter<std::vector<double>>("mpc_sampler.curvature_samples", {-0.2, -0.1, 0.0, 0.1, 0.2});
  declare_parameter<int>("mpc_sampler.sample_count", 5);
  declare_parameter<double>("mpc_sampler.max_curvature_1pm", 0.2);
  declare_parameter<double>("mpc_sampler.lateral_error_weight", 1.0);
  declare_parameter<double>("mpc_sampler.heading_error_weight", 0.5);
  declare_parameter<double>("mpc_sampler.speed_error_weight", 0.2);
  declare_parameter<double>("mpc_sampler.steering_effort_weight", 0.05);
  declare_parameter<double>("command_smoother.max_speed_step_mps", 0.05);
  declare_parameter<double>("command_smoother.max_steer_rate_radps", 0.35);
  declare_parameter<double>("scu.max_steering_angle_deg", 30.0);
  declare_parameter<double>("scu.max_target_speed_kmh", 5.0);
  declare_parameter<double>("scu.front_steer_sign", 1.0);
  declare_parameter<double>("scu.rear_steer_sign", 1.0);
  declare_parameter<int>("scu.stop_shift_level", 1);
  declare_parameter<int>("scu.torque_or_speed_mode", 1);
  declare_parameter<bool>("scu.steering_angle_speed_valid", false);
  declare_parameter<bool>("scu.brake_force_command_valid", false);
  declare_parameter<int>("scu.lights.left", 0);
  declare_parameter<int>("scu.lights.right", 0);
  declare_parameter<int>("scu.lights.position", 0);
  declare_parameter<int>("scu.lights.low_beam", 0);

  load_runtime_options();

  pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
    get_parameter("topics.localization_pose_topic").as_string(), 10,
    [this](geometry_msgs::msg::PoseStamped::SharedPtr msg) { on_pose(msg); });
  trajectory_sub_ = create_subscription<low_speed_av_interfaces::msg::Trajectory>(
    get_parameter("topics.trajectory_topic").as_string(), 10,
    [this](low_speed_av_interfaces::msg::Trajectory::SharedPtr msg) { on_trajectory(msg); });
  vehicle_state_sub_ = create_subscription<low_speed_av_interfaces::msg::VehicleState>(
    get_parameter("topics.vehicle_state_topic").as_string(), 10,
    [this](low_speed_av_interfaces::msg::VehicleState::SharedPtr msg) { on_vehicle_state(msg); });
  safety_status_sub_ = create_subscription<low_speed_av_interfaces::msg::ModuleStatus>(
    get_parameter("topics.safety_status_topic").as_string(), 10,
    [this](low_speed_av_interfaces::msg::ModuleStatus::SharedPtr msg) { on_safety_status(msg); });
  command_pub_ = create_publisher<low_speed_av_interfaces::msg::ControlCommand>(
    get_parameter("topics.control_command_topic").as_string(), 10);
  scu_command_pub_ = create_publisher<chassis_interfaces::msg::ScuControlCommand>(
    get_parameter("topics.scu_command_topic").as_string(), 10);
  status_pub_ = create_publisher<low_speed_av_interfaces::msg::ModuleStatus>(
    get_parameter("topics.control_status_topic").as_string(), 10);
  set_algorithm_srv_ = create_service<low_speed_av_interfaces::srv::SetControllerAlgorithm>(
    "~/set_controller_algorithm",
    [this](
      const std::shared_ptr<low_speed_av_interfaces::srv::SetControllerAlgorithm::Request> request,
      std::shared_ptr<low_speed_av_interfaces::srv::SetControllerAlgorithm::Response> response) {
      on_set_controller_algorithm(request, response);
    });

  const double rate_hz = std::max(1.0, get_parameter("controller.control_rate_hz").as_double());
  smoother_options_.dt_s = 1.0 / rate_hz;
  controller_options_.control_dt_s = smoother_options_.dt_s;
  timer_ = create_wall_timer(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::duration<double>(smoother_options_.dt_s)),
    [this]() { on_timer(); });
  publish_status("active", 0, "control node ready");
}

void ControlNode::load_runtime_options()
{
  output_mode_ = get_parameter("output.mode").as_string();
  controller_algorithm_ = get_parameter("controller.algorithm").as_string();
  vehicle_model_name_ = get_parameter("vehicle.model").as_string();
  controller_ = ControllerFactory::create(controller_algorithm_);
  vehicle_model_ = VehicleModelFactory::create(vehicle_model_name_);
  safety_estop_latched_ = get_parameter("safety.estop_latched").as_bool();
  safety_clear_level_ = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("safety.clear_level").as_int()));
  safety_clear_state_ = get_parameter("safety.clear_state").as_string();

  vehicle_limits_.wheel_base_m = get_parameter("vehicle.wheel_base_m").as_double();
  vehicle_limits_.max_speed_mps = get_parameter("vehicle.max_speed_mps").as_double();
  vehicle_limits_.max_accel_mps2 = get_parameter("vehicle.max_accel_mps2").as_double();
  vehicle_limits_.max_decel_mps2 = get_parameter("vehicle.max_decel_mps2").as_double();
  vehicle_limits_.max_front_steer_rad = get_parameter("vehicle.max_front_steer_rad").as_double();
  vehicle_limits_.max_rear_steer_rad = get_parameter("vehicle.max_rear_steer_rad").as_double();
  vehicle_limits_.max_front_steer_rate_radps = get_parameter("vehicle.max_front_steer_rate_radps").as_double();
  vehicle_limits_.max_rear_steer_rate_radps = get_parameter("vehicle.max_rear_steer_rate_radps").as_double();
  vehicle_limits_.rear_steer_ratio = get_parameter("vehicle.rear_steer_ratio").as_double();
  controller_options_.wheel_base_m = vehicle_limits_.wheel_base_m;
  controller_options_.control_dt_s = 1.0 /
    std::max(1.0, get_parameter("controller.control_rate_hz").as_double());

  controller_options_.lookahead_min_m = get_parameter("pure_pursuit.lookahead_min_m").as_double();
  controller_options_.lookahead_max_m = get_parameter("pure_pursuit.lookahead_max_m").as_double();
  controller_options_.lookahead_speed_gain = get_parameter("pure_pursuit.lookahead_speed_gain").as_double();
  controller_options_.stanley_k = get_parameter("stanley.k").as_double();
  controller_options_.stanley_epsilon_mps = get_parameter("stanley.epsilon_mps").as_double();
  controller_options_.max_correction_rad = get_parameter("stanley.max_correction_rad").as_double();
  controller_options_.lqr_q_lateral_error = get_parameter("lqr.q_lateral_error").as_double();
  controller_options_.lqr_q_heading_error = get_parameter("lqr.q_heading_error").as_double();
  controller_options_.lqr_r_steering = get_parameter("lqr.r_steering").as_double();
  controller_options_.lqr_max_iterations = get_parameter("lqr.max_iterations").as_int();
  controller_options_.lqr_convergence_eps = get_parameter("lqr.convergence_eps").as_double();
  controller_options_.lqr_min_speed_mps = get_parameter("lqr.min_speed_mps").as_double();
  controller_options_.lqr_preview_time_s = get_parameter("lqr.preview_time_s").as_double();
  controller_options_.lqr_use_curvature_feedforward =
    get_parameter("lqr.use_curvature_feedforward").as_bool();
  controller_options_.lqr_max_steering_angle_rad =
    get_parameter("lqr.max_steering_angle_rad").as_double();
  controller_options_.mpc_horizon_steps = get_parameter("mpc_sampler.horizon_steps").as_int();
  controller_options_.mpc_dt_s = get_parameter("mpc_sampler.dt_s").as_double();
  controller_options_.mpc_curvature_samples =
    get_parameter("mpc_sampler.curvature_samples").as_double_array();
  controller_options_.mpc_sample_count = get_parameter("mpc_sampler.sample_count").as_int();
  controller_options_.mpc_max_curvature_1pm = get_parameter("mpc_sampler.max_curvature_1pm").as_double();
  controller_options_.mpc_lateral_error_weight = get_parameter("mpc_sampler.lateral_error_weight").as_double();
  controller_options_.mpc_heading_error_weight = get_parameter("mpc_sampler.heading_error_weight").as_double();
  controller_options_.mpc_speed_error_weight = get_parameter("mpc_sampler.speed_error_weight").as_double();
  controller_options_.mpc_steering_effort_weight = get_parameter("mpc_sampler.steering_effort_weight").as_double();

  smoother_options_.max_speed_step_mps = get_parameter("command_smoother.max_speed_step_mps").as_double();
  smoother_options_.max_steer_rate_radps = get_parameter("command_smoother.max_steer_rate_radps").as_double();

  scu_options_.max_steering_angle_deg = get_parameter("scu.max_steering_angle_deg").as_double();
  scu_options_.max_target_speed_kmh = get_parameter("scu.max_target_speed_kmh").as_double();
  scu_options_.front_steer_sign = get_parameter("scu.front_steer_sign").as_double();
  scu_options_.rear_steer_sign = get_parameter("scu.rear_steer_sign").as_double();
  scu_options_.stop_shift_level = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.stop_shift_level").as_int()));
  scu_options_.torque_or_speed_mode = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.torque_or_speed_mode").as_int()));
  scu_options_.steering_angle_speed_valid =
    get_parameter("scu.steering_angle_speed_valid").as_bool();
  scu_options_.brake_force_command_valid =
    get_parameter("scu.brake_force_command_valid").as_bool();
  scu_options_.left_turn_light = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.lights.left").as_int()));
  scu_options_.right_turn_light = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.lights.right").as_int()));
  scu_options_.position_light = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.lights.position").as_int()));
  scu_options_.low_beam = static_cast<uint8_t>(
    std::max<int64_t>(0, get_parameter("scu.lights.low_beam").as_int()));
}

ControlCommand ControlNode::controlled_stop(const std::string & reason) const
{
  ControlCommand cmd;
  cmd.speed_mps = 0.0;
  cmd.acceleration_mps2 = -std::min(0.5, vehicle_limits_.max_decel_mps2);
  cmd.brake = 1.0;
  cmd.gear = state_.gear;
  cmd.enable = false;
  cmd.emergency_stop = true;
  cmd.controller_algorithm = controller_algorithm_;
  cmd.vehicle_model = vehicle_model_name_;
  cmd.reason = reason;
  return cmd;
}

void ControlNode::on_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
{
  pose_.x_m = msg->pose.position.x;
  pose_.y_m = msg->pose.position.y;
  const auto & q = msg->pose.orientation;
  pose_.yaw_rad = std::atan2(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  last_pose_time_ = now();
  have_pose_ = true;
}

void ControlNode::on_trajectory(const low_speed_av_interfaces::msg::Trajectory::SharedPtr msg)
{
  trajectory_.clear();
  for (const auto & p : msg->points) {
    trajectory_.push_back({p.x_m, p.y_m, p.yaw_rad, p.kappa_1pm, p.s_m, p.v_mps, p.gear});
  }
  last_trajectory_time_ = now();
  have_trajectory_ = true;
}

void ControlNode::on_vehicle_state(const low_speed_av_interfaces::msg::VehicleState::SharedPtr msg)
{
  state_.speed_mps = msg->speed_mps;
  state_.acceleration_mps2 = msg->acceleration_mps2;
  state_.front_steering_angle_rad = msg->front_steering_angle_rad;
  state_.rear_steering_angle_rad = msg->rear_steering_angle_rad;
  state_.gear = msg->gear;
  state_.autonomous_enabled = msg->autonomous_enabled;
}

void ControlNode::on_safety_status(const low_speed_av_interfaces::msg::ModuleStatus::SharedPtr msg)
{
  const bool requested = msg->level >= 2 || msg->state == "estop" ||
    msg->state == "emergency_stop" || msg->state == "failure";
  const bool clear_requested = msg->level <= safety_clear_level_ &&
    (msg->state == safety_clear_state_ || msg->state == "clear" || msg->state == "ok" || msg->state == "standby");
  if (requested) {
    safety_estop_active_ = true;
    publish_status("safety_estop", 2, msg->message);
  } else if (!safety_estop_latched_ || clear_requested) {
    safety_estop_active_ = false;
    publish_status("active", 0, "safety estop clear");
  }
}

ControlCommand ControlNode::compute_tracking_command()
{
  auto raw = controller_->compute(pose_, state_, trajectory_, controller_options_);
  if (raw.emergency_stop) {
    raw.controller_algorithm = controller_algorithm_;
    raw.vehicle_model = vehicle_model_name_;
    return raw;
  }

  auto steering = vehicle_model_->steering_from_curvature(raw.desired_curvature_1pm, vehicle_limits_);
  steering.speed_mps = raw.speed_mps;
  steering.acceleration_mps2 = raw.acceleration_mps2;
  steering.desired_curvature_1pm = raw.desired_curvature_1pm;
  steering.brake = raw.brake;
  steering.gear = raw.gear;
  steering.enable = true;
  steering.emergency_stop = false;
  steering.controller_algorithm = controller_algorithm_;
  steering.vehicle_model = vehicle_model_name_;
  steering.reason = "tracking";
  return steering;
}

ControlCommand ControlNode::finalize_command(const ControlCommand & command)
{
  return smoother_.smooth(limiter_.limit(command, vehicle_limits_), smoother_options_);
}

void ControlNode::on_timer()
{
  const auto now_time = now();
  if (safety_estop_active_) {
    publish_command(finalize_command(controlled_stop("safety_estop")));
    return;
  }
  if (!have_pose_ ||
    (now_time - last_pose_time_).seconds() > get_parameter("controller.localization_timeout_s").as_double())
  {
    publish_status("stopping", 1, "localization_timeout");
    publish_command(finalize_command(controlled_stop("localization_timeout")));
    return;
  }
  if (!have_trajectory_ ||
    (now_time - last_trajectory_time_).seconds() > get_parameter("controller.trajectory_timeout_s").as_double())
  {
    publish_status("stopping", 1, "trajectory_timeout");
    publish_command(finalize_command(controlled_stop("trajectory_timeout")));
    return;
  }
  if (trajectory_.empty()) {
    publish_status("stopping", 1, "empty_trajectory");
    publish_command(finalize_command(controlled_stop("empty_trajectory")));
    return;
  }

  publish_command(finalize_command(compute_tracking_command()));
}

void ControlNode::on_set_controller_algorithm(
  const std::shared_ptr<low_speed_av_interfaces::srv::SetControllerAlgorithm::Request> request,
  std::shared_ptr<low_speed_av_interfaces::srv::SetControllerAlgorithm::Response> response)
{
  try {
    const auto next_controller_name = request->controller_algorithm.empty() ?
      controller_algorithm_ : request->controller_algorithm;
    const auto next_vehicle_model_name = request->vehicle_model.empty() ?
      vehicle_model_name_ : request->vehicle_model;
    auto next_controller = ControllerFactory::create(next_controller_name);
    auto next_vehicle_model = VehicleModelFactory::create(next_vehicle_model_name);
    controller_algorithm_ = next_controller_name;
    vehicle_model_name_ = next_vehicle_model_name;
    controller_ = std::move(next_controller);
    vehicle_model_ = std::move(next_vehicle_model);
    response->success = true;
    response->message = "controller algorithm updated";
    publish_status("active", 0, response->message);
  } catch (const std::exception & e) {
    response->success = false;
    response->message = e.what();
    publish_status("failure", 2, response->message);
  }
}

void ControlNode::publish_status(const std::string & state, uint8_t level, const std::string & message)
{
  low_speed_av_interfaces::msg::ModuleStatus status;
  status.header.stamp = now();
  status.module_name = "low_speed_av_control";
  status.state = state;
  status.level = level;
  status.message = message;
  status_pub_->publish(status);
}

void ControlNode::publish_command(const ControlCommand & command)
{
  low_speed_av_interfaces::msg::ControlCommand msg;
  msg.header.stamp = now();
  msg.speed_mps = command.speed_mps;
  msg.acceleration_mps2 = command.acceleration_mps2;
  msg.steering_angle_rad = command.steering_angle_rad;
  msg.front_steering_angle_rad = command.front_steering_angle_rad;
  msg.rear_steering_angle_rad = command.rear_steering_angle_rad;
  msg.brake = command.brake;
  msg.gear = static_cast<int8_t>(command.gear);
  msg.enable = command.enable;
  msg.emergency_stop = command.emergency_stop;
  msg.controller_algorithm = command.controller_algorithm;
  msg.vehicle_model = command.vehicle_model;
  msg.reason = command.reason;
  if (publish_internal_command()) {
    command_pub_->publish(msg);
  }
  if (publish_scu_command()) {
    const auto result = scu_mapper_.map(command, scu_options_);
    for (const auto & warning : result.warnings) {
      RCLCPP_WARN(get_logger(), "%s", warning.c_str());
    }
    scu_command_pub_->publish(result.command);
  }
}

bool ControlNode::publish_internal_command() const
{
  return output_mode_ == "internal" || output_mode_ == "both";
}

bool ControlNode::publish_scu_command() const
{
  return output_mode_ != "internal";
}

}  // namespace low_speed_av_control
