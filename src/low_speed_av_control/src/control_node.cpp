#include "low_speed_av_control/control_node.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace low_speed_av_control {
namespace {

void require_positive(const std::string & name, double value)
{
  if (!std::isfinite(value) || value <= 0.0) {
    throw std::invalid_argument(name + " must be finite and > 0");
  }
}

void require_nonnegative(const std::string & name, double value)
{
  if (!std::isfinite(value) || value < 0.0) {
    throw std::invalid_argument(name + " must be finite and >= 0");
  }
}

std::string lowercase(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

bool status_requests_estop(const low_speed_av_interfaces::msg::ModuleStatus & msg)
{
  const auto state = lowercase(msg.state);
  return msg.level >= 2U || state == "estop" || state == "emergency_stop" || state == "failure";
}

bool trajectory_status_is_emergency(const std::string & status)
{
  const auto value = lowercase(status);
  return value.find("emergency") != std::string::npos || value.find("estop") != std::string::npos;
}

}  // namespace

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
  declare_parameter<std::string>("output.mode", "both");
  declare_parameter<bool>("safety.estop_latched", true);
  declare_parameter<double>("safety.clear_speed_threshold_mps", 0.05);
  declare_parameter<std::string>("controller.algorithm", "pure_pursuit");
  declare_parameter<double>("controller.control_rate_hz", 50.0);
  declare_parameter<double>("controller.localization_timeout_s", 0.2);
  declare_parameter<double>("controller.trajectory_timeout_s", 0.5);
  declare_parameter<std::vector<std::string>>(
    "controller.allowed_trajectory_statuses", std::vector<std::string>{"ok"});
  declare_parameter<double>("controller.trajectory_s_tolerance_m", 1.0e-4);
  declare_parameter<double>("control.status_publish_rate_hz", 5.0);
  declare_parameter<bool>("vehicle_state.required", false);
  declare_parameter<double>("vehicle_state.timeout_s", 0.5);
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
  declare_parameter<std::vector<double>>(
    "mpc_sampler.curvature_samples", {-0.2, -0.1, 0.0, 0.1, 0.2});
  declare_parameter<int>("mpc_sampler.sample_count", 5);
  declare_parameter<double>("mpc_sampler.max_curvature_1pm", 0.2);
  declare_parameter<double>("mpc_sampler.lateral_error_weight", 1.0);
  declare_parameter<double>("mpc_sampler.heading_error_weight", 0.5);
  declare_parameter<double>("mpc_sampler.speed_error_weight", 0.2);
  declare_parameter<double>("mpc_sampler.steering_effort_weight", 0.05);
  declare_parameter<double>("command_smoother.max_speed_step_mps", 0.05);
  declare_parameter<double>("command_smoother.max_steer_rate_radps", 0.35);
  declare_parameter<double>("scu.max_steering_angle_deg", 27.0);
  declare_parameter<double>("scu.max_target_speed_kmh", 5.0);
  declare_parameter<double>("scu.front_steer_sign", 1.0);
  declare_parameter<double>("scu.rear_steer_sign", 1.0);
  declare_parameter<std::string>("scu.overrange_policy", "clamp");
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
  clear_estop_srv_ = create_service<std_srvs::srv::Trigger>(
    "~/clear_estop",
    [this](
      const std::shared_ptr<std_srvs::srv::Trigger::Request> request,
      std::shared_ptr<std_srvs::srv::Trigger::Response> response) {
      on_clear_estop(request, response);
    });

  const double rate_hz = get_parameter("controller.control_rate_hz").as_double();
  smoother_options_.dt_s = 1.0 / rate_hz;
  controller_options_.control_dt_s = smoother_options_.dt_s;
  const auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::duration<double>(smoother_options_.dt_s));
  timer_ = create_wall_timer(period, [this]() { on_timer(); });
  publish_status("WAIT_INPUTS", 1, "control node ready; waiting for inputs");
}

void ControlNode::load_runtime_options()
{
  output_mode_ = get_parameter("output.mode").as_string();
  if (output_mode_ != "internal" && output_mode_ != "scu_control_command" && output_mode_ != "both") {
    throw std::invalid_argument("output.mode must be internal, scu_control_command, or both");
  }
  const double control_rate_hz = get_parameter("controller.control_rate_hz").as_double();
  require_positive("controller.control_rate_hz", control_rate_hz);
  require_positive(
    "controller.localization_timeout_s", get_parameter("controller.localization_timeout_s").as_double());
  require_positive(
    "controller.trajectory_timeout_s", get_parameter("controller.trajectory_timeout_s").as_double());
  require_positive(
    "control.status_publish_rate_hz", get_parameter("control.status_publish_rate_hz").as_double());

  safety_estop_latched_ = get_parameter("safety.estop_latched").as_bool();
  safety_clear_speed_threshold_mps_ = get_parameter("safety.clear_speed_threshold_mps").as_double();
  require_nonnegative("safety.clear_speed_threshold_mps", safety_clear_speed_threshold_mps_);
  allowed_trajectory_statuses_ = get_parameter("controller.allowed_trajectory_statuses").as_string_array();
  if (allowed_trajectory_statuses_.empty() ||
    std::any_of(allowed_trajectory_statuses_.begin(), allowed_trajectory_statuses_.end(),
    [](const auto & value) {return value.empty();}))
  {
    throw std::invalid_argument("controller.allowed_trajectory_statuses must contain non-empty statuses");
  }
  trajectory_s_tolerance_m_ = get_parameter("controller.trajectory_s_tolerance_m").as_double();
  require_nonnegative("controller.trajectory_s_tolerance_m", trajectory_s_tolerance_m_);
  vehicle_state_required_ = get_parameter("vehicle_state.required").as_bool();
  vehicle_state_timeout_s_ = get_parameter("vehicle_state.timeout_s").as_double();
  require_positive("vehicle_state.timeout_s", vehicle_state_timeout_s_);

  controller_algorithm_ = get_parameter("controller.algorithm").as_string();
  vehicle_model_name_ = get_parameter("vehicle.model").as_string();
  controller_ = ControllerFactory::create(controller_algorithm_);
  vehicle_model_ = VehicleModelFactory::create(vehicle_model_name_);

  vehicle_limits_.wheel_base_m = get_parameter("vehicle.wheel_base_m").as_double();
  vehicle_limits_.max_speed_mps = get_parameter("vehicle.max_speed_mps").as_double();
  vehicle_limits_.max_accel_mps2 = get_parameter("vehicle.max_accel_mps2").as_double();
  vehicle_limits_.max_decel_mps2 = get_parameter("vehicle.max_decel_mps2").as_double();
  vehicle_limits_.max_front_steer_rad = get_parameter("vehicle.max_front_steer_rad").as_double();
  vehicle_limits_.max_rear_steer_rad = get_parameter("vehicle.max_rear_steer_rad").as_double();
  vehicle_limits_.max_front_steer_rate_radps =
    get_parameter("vehicle.max_front_steer_rate_radps").as_double();
  vehicle_limits_.max_rear_steer_rate_radps =
    get_parameter("vehicle.max_rear_steer_rate_radps").as_double();
  vehicle_limits_.rear_steer_ratio = get_parameter("vehicle.rear_steer_ratio").as_double();
  require_positive("vehicle.wheel_base_m", vehicle_limits_.wheel_base_m);
  require_nonnegative("vehicle.max_speed_mps", vehicle_limits_.max_speed_mps);
  require_nonnegative("vehicle.max_accel_mps2", vehicle_limits_.max_accel_mps2);
  require_nonnegative("vehicle.max_decel_mps2", vehicle_limits_.max_decel_mps2);
  require_nonnegative("vehicle.max_front_steer_rad", vehicle_limits_.max_front_steer_rad);
  require_nonnegative("vehicle.max_rear_steer_rad", vehicle_limits_.max_rear_steer_rad);
  require_nonnegative("vehicle.max_front_steer_rate_radps", vehicle_limits_.max_front_steer_rate_radps);
  require_nonnegative("vehicle.max_rear_steer_rate_radps", vehicle_limits_.max_rear_steer_rate_radps);
  require_nonnegative("vehicle.rear_steer_ratio", vehicle_limits_.rear_steer_ratio);

  controller_options_.wheel_base_m = vehicle_limits_.wheel_base_m;
  controller_options_.control_dt_s = 1.0 / control_rate_hz;
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
  controller_options_.mpc_lateral_error_weight =
    get_parameter("mpc_sampler.lateral_error_weight").as_double();
  controller_options_.mpc_heading_error_weight =
    get_parameter("mpc_sampler.heading_error_weight").as_double();
  controller_options_.mpc_speed_error_weight =
    get_parameter("mpc_sampler.speed_error_weight").as_double();
  controller_options_.mpc_steering_effort_weight =
    get_parameter("mpc_sampler.steering_effort_weight").as_double();

  smoother_options_.max_speed_step_mps = get_parameter("command_smoother.max_speed_step_mps").as_double();
  smoother_options_.max_steer_rate_radps = get_parameter("command_smoother.max_steer_rate_radps").as_double();
  require_nonnegative("command_smoother.max_speed_step_mps", smoother_options_.max_speed_step_mps);
  require_nonnegative("command_smoother.max_steer_rate_radps", smoother_options_.max_steer_rate_radps);

  scu_options_.max_steering_angle_deg = get_parameter("scu.max_steering_angle_deg").as_double();
  scu_options_.max_target_speed_kmh = get_parameter("scu.max_target_speed_kmh").as_double();
  scu_options_.front_steer_sign = get_parameter("scu.front_steer_sign").as_double();
  scu_options_.rear_steer_sign = get_parameter("scu.rear_steer_sign").as_double();
  scu_options_.overrange_policy = get_parameter("scu.overrange_policy").as_string();
  const auto stop_shift = get_parameter("scu.stop_shift_level").as_int();
  const auto torque_mode = get_parameter("scu.torque_or_speed_mode").as_int();
  if (stop_shift < 1 || stop_shift > 3) {
    throw std::invalid_argument("scu.stop_shift_level must be 1(D), 2(N), or 3(R)");
  }
  if (torque_mode < 0 || torque_mode > 1) {
    throw std::invalid_argument("scu.torque_or_speed_mode must be 0 or 1");
  }
  require_positive("scu.max_steering_angle_deg", scu_options_.max_steering_angle_deg);
  require_nonnegative("scu.max_target_speed_kmh", scu_options_.max_target_speed_kmh);
  if (!std::isfinite(scu_options_.front_steer_sign) || !std::isfinite(scu_options_.rear_steer_sign)) {
    throw std::invalid_argument("SCU steering signs must be finite");
  }
  if (scu_options_.overrange_policy != "clamp" && scu_options_.overrange_policy != "zero") {
    throw std::invalid_argument("scu.overrange_policy must be clamp or zero");
  }
  scu_options_.stop_shift_level = static_cast<uint8_t>(stop_shift);
  scu_options_.torque_or_speed_mode = static_cast<uint8_t>(torque_mode);
  scu_options_.steering_angle_speed_valid = get_parameter("scu.steering_angle_speed_valid").as_bool();
  scu_options_.brake_force_command_valid = get_parameter("scu.brake_force_command_valid").as_bool();
  scu_options_.left_turn_light = static_cast<uint8_t>(get_parameter("scu.lights.left").as_int());
  scu_options_.right_turn_light = static_cast<uint8_t>(get_parameter("scu.lights.right").as_int());
  scu_options_.position_light = static_cast<uint8_t>(get_parameter("scu.lights.position").as_int());
  scu_options_.low_beam = static_cast<uint8_t>(get_parameter("scu.lights.low_beam").as_int());
}

ControlCommand ControlNode::controlled_stop(const std::string & reason, bool emergency_stop) const
{
  ControlCommand cmd;
  cmd.speed_mps = 0.0;
  cmd.acceleration_mps2 = -std::min(0.5, vehicle_limits_.max_decel_mps2);
  cmd.desired_curvature_1pm = 0.0;
  cmd.steering_angle_rad = 0.0;
  cmd.front_steering_angle_rad = 0.0;
  cmd.rear_steering_angle_rad = 0.0;
  cmd.brake = 1.0;
  cmd.gear = state_.gear >= 1 && state_.gear <= 3 ? state_.gear : 1;
  cmd.enable = false;
  cmd.emergency_stop = emergency_stop;
  cmd.controller_algorithm = controller_algorithm_;
  cmd.vehicle_model = vehicle_model_name_;
  cmd.reason = reason.empty() ? "controlled_stop" : reason;
  return cmd;
}

void ControlNode::on_pose(const geometry_msgs::msg::PoseStamped::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  pose_.x_m = msg->pose.position.x;
  pose_.y_m = msg->pose.position.y;
  const auto & q = msg->pose.orientation;
  pose_.yaw_rad = std::atan2(
    2.0 * (q.w * q.z + q.x * q.y),
    1.0 - 2.0 * (q.y * q.y + q.z * q.z));
  last_pose_receive_time_ = std::chrono::steady_clock::now();
  have_pose_ = true;
  pose_valid_ = std::isfinite(pose_.x_m) && std::isfinite(pose_.y_m) && std::isfinite(pose_.yaw_rad);
}

void ControlNode::on_trajectory(const low_speed_av_interfaces::msg::Trajectory::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  trajectory_id_ = msg->trajectory_id;
  trajectory_source_package_id_ = msg->source_package_id;
  trajectory_status_ = msg->status;
  trajectory_emergency_active_ = msg->emergency_stop || trajectory_status_is_emergency(msg->status);
  trajectory_.clear();
  trajectory_.reserve(msg->points.size());
  for (const auto & p : msg->points) {
    trajectory_.push_back({p.x_m, p.y_m, p.yaw_rad, p.kappa_1pm, p.s_m, p.v_mps, p.gear});
  }
  trajectory_valid_ = validate_trajectory_input(
    trajectory_, trajectory_id_, trajectory_source_package_id_, trajectory_status_, msg->emergency_stop,
    allowed_trajectory_statuses_, trajectory_s_tolerance_m_, &trajectory_invalid_reason_);
  last_trajectory_receive_time_ = std::chrono::steady_clock::now();
  have_trajectory_ = true;
}

void ControlNode::on_vehicle_state(const low_speed_av_interfaces::msg::VehicleState::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  state_.speed_mps = msg->speed_mps;
  state_.acceleration_mps2 = msg->acceleration_mps2;
  state_.front_steering_angle_rad = msg->front_steering_angle_rad;
  state_.rear_steering_angle_rad = msg->rear_steering_angle_rad;
  state_.gear = msg->gear;
  state_.autonomous_enabled = msg->autonomous_enabled;
  state_.brake_pressed = msg->brake_pressed;
  state_.fault_code = msg->fault_code;
  vehicle_state_valid_ = vehicle_state_is_finite(state_) && state_.gear >= 0 && state_.gear <= 3;
  have_vehicle_state_ = true;
  last_vehicle_state_receive_time_ = std::chrono::steady_clock::now();
}

void ControlNode::on_safety_status(const low_speed_av_interfaces::msg::ModuleStatus::SharedPtr msg)
{
  if (!msg) {
    return;
  }
  safety_request_active_ = status_requests_estop(*msg);
  safety_estop_latch_.update(safety_request_active_, safety_estop_latched_);
  if (safety_request_active_) {
    publish_status("ESTOP_LATCHED", 2, msg->message.empty() ? "safety estop requested" : msg->message);
  }
}

SafetyInputs ControlNode::make_safety_inputs() const
{
  const auto steady_now = std::chrono::steady_clock::now();
  SafetyInputs inputs;
  inputs.safety_request_active = safety_request_active_;
  inputs.safety_estop_latched = safety_estop_latch_.is_latched();
  inputs.trajectory_emergency = trajectory_emergency_active_;
  inputs.have_pose = have_pose_;
  inputs.pose_valid = pose_valid_;
  inputs.pose_timed_out = have_pose_ &&
    std::chrono::duration<double>(steady_now - last_pose_receive_time_).count() >
    get_parameter("controller.localization_timeout_s").as_double();
  inputs.have_trajectory = have_trajectory_;
  inputs.trajectory_valid = trajectory_valid_;
  inputs.trajectory_timed_out = have_trajectory_ &&
    std::chrono::duration<double>(steady_now - last_trajectory_receive_time_).count() >
    get_parameter("controller.trajectory_timeout_s").as_double();
  inputs.trajectory_invalid_reason = trajectory_invalid_reason_;
  inputs.vehicle_state_required = vehicle_state_required_;
  inputs.have_vehicle_state = have_vehicle_state_;
  inputs.vehicle_state_valid = vehicle_state_valid_;
  inputs.vehicle_state_timed_out = have_vehicle_state_ &&
    std::chrono::duration<double>(steady_now - last_vehicle_state_receive_time_).count() >
    vehicle_state_timeout_s_;
  inputs.autonomous_enabled = state_.autonomous_enabled;
  inputs.brake_pressed = state_.brake_pressed;
  inputs.fault_code = state_.fault_code;
  inputs.force_ready_cycle = force_ready_cycle_;
  return inputs;
}

bool ControlNode::inputs_ready_for_estop_clear(std::string * reason) const
{
  const auto inputs = make_safety_inputs();
  const EstopClearInputs clear_inputs{
    safety_request_active_, trajectory_emergency_active_,
    inputs.have_pose && inputs.pose_valid && !inputs.pose_timed_out,
    inputs.have_trajectory && inputs.trajectory_valid && !inputs.trajectory_timed_out,
    inputs.vehicle_state_required, inputs.have_vehicle_state,
    inputs.vehicle_state_valid && !inputs.vehicle_state_timed_out,
    state_.speed_mps, safety_clear_speed_threshold_mps_, state_.autonomous_enabled,
    state_.brake_pressed, state_.fault_code};
  const auto decision = evaluate_estop_clear(clear_inputs);
  if (reason) {
    *reason = decision.allowed ? std::string() : decision.reason;
  }
  return decision.allowed;
}

void ControlNode::on_clear_estop(
  const std::shared_ptr<std_srvs::srv::Trigger::Request>,
  std::shared_ptr<std_srvs::srv::Trigger::Response> response)
{
  if (!safety_estop_latch_.is_latched()) {
    response->success = true;
    response->message = "no safety estop is latched";
    return;
  }
  std::string reason;
  if (!inputs_ready_for_estop_clear(&reason)) {
    response->success = false;
    response->message = "estop clear rejected: " + reason;
    publish_status("ESTOP_LATCHED", 2, response->message);
    return;
  }
  safety_estop_latch_.clear_explicit();
  force_ready_cycle_ = true;
  safety_state_ = ControlSafetyState::Ready;
  response->success = true;
  response->message = "estop cleared; state is READY and inputs will be re-evaluated next cycle";
  publish_status("READY", 0, response->message);
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
  const auto decision = safety_state_machine_.evaluate(make_safety_inputs());
  safety_state_ = decision.state;
  if (force_ready_cycle_) {
    force_ready_cycle_ = false;
  }

  if (!decision.allow_tracking) {
    const auto command = finalize_command(controlled_stop(decision.reason, decision.emergency_stop));
    publish_command(command);
    publish_periodic_status(
      ControlSafetyStateMachine::state_name(decision.state), decision.level, decision.reason);
    return;
  }

  const auto command = finalize_command(compute_tracking_command());
  if (command.emergency_stop || command.brake > 0.0 || !command.enable) {
    safety_state_ = command.emergency_stop ?
      ControlSafetyState::EstopLatched : ControlSafetyState::ControlledStop;
    publish_command(finalize_command(controlled_stop(
      command.reason.empty() ? "controller_requested_stop" : command.reason,
      command.emergency_stop)));
    publish_periodic_status("CONTROLLED_STOP", command.emergency_stop ? 2 : 1, command.reason);
    return;
  }
  publish_command(command);
  publish_periodic_status("ACTIVE", 0, "tracking trajectory " + trajectory_id_);
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
    publish_status(ControlSafetyStateMachine::state_name(safety_state_), 0, response->message);
  } catch (const std::exception & e) {
    response->success = false;
    response->message = e.what();
    publish_status("CONTROLLED_STOP", 2, response->message);
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
  last_status_time_ = status.header.stamp;
  status_pub_->publish(status);
}

void ControlNode::publish_periodic_status(
  const std::string & state,
  uint8_t level,
  const std::string & message)
{
  const double rate_hz = get_parameter("control.status_publish_rate_hz").as_double();
  const auto now_time = now();
  if (last_status_time_.nanoseconds() != 0 &&
    (now_time - last_status_time_).seconds() < 1.0 / rate_hz)
  {
    return;
  }
  publish_status(state, level, message);
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
  return output_mode_ == "scu_control_command" || output_mode_ == "both";
}

}  // namespace low_speed_av_control
