#include "chassis_driver/control_command_bridge.hpp"

#include "chassis_driver/chassis_driver_node.hpp"
#include "chassis_driver/dbc_protocol.hpp"
#include "chassis_driver/scu_control_frame_builder.hpp"

namespace chassis_driver
{

/** Subscribe control topics and convert each message into DBC-encoded CAN frame. */
/** 订阅控制话题，并将每条消息转换为按 DBC 编码的 CAN 帧。 */
ControlCommandBridge::ControlCommandBridge(ChassisDriverNode & node)
: node_(node)
{
  auto qos = rclcpp::QoS(rclcpp::KeepLast(node_.default_qos_depth_));

  if (node_.isSubscribeTopicEnabled("control_scu_control_command")) {
    node_.scu_control_sub_ = node_.create_subscription<chassis_interfaces::msg::ScuControlCommand>(
      node_.makeTopicName("control/scu_control_command"), qos,
      [this](const chassis_interfaces::msg::ScuControlCommand::SharedPtr msg) {
      // Engineering assumption: although message is named SCU_*, ACU is allowed to send it.
      // 工程假设：虽然报文名为 SCU_*，但按当前需求 ACU 被允许发送该报文。
        ScuControlFrameInput input;
        input.shift_level = msg->scu_shift_level_request;
        input.front_steering_angle_deg = msg->scu_steering_angle_front;
        input.rear_steering_angle_deg = msg->scu_steering_angle_rear;
        input.target_speed_kmh = msg->scu_target_speed;
        input.brake_enable = msg->scu_brake_enable;
        input.left_turn_light_request = msg->gw_left_turn_light_request;
        input.right_turn_light_request = msg->gw_right_turn_light_request;
        input.position_light_request = msg->gw_position_light_request;
        input.low_beam_request = msg->gw_low_beam_request;
        input.torque_or_speed_mode = msg->scu_torque_or_speed_mode;
        input.steering_angle_speed_valid = msg->steering_angle_speed_valid;
        input.brake_force_command_valid = msg->brake_force_command_valid;
        const auto result = build_scu_control_frame(
          input,
          {node_.scu_control_max_steering_angle_deg_, node_.scu_control_max_target_speed_kmh_});
        if (!result.valid) {
          RCLCPP_WARN(
            node_.get_logger(), "Drop SCU_Control_Command: %s", result.rejection_reason.c_str());
          return;
        }
        for (const auto & warning : result.warnings) {
          RCLCPP_WARN(node_.get_logger(), "SCU_Control_Command: %s", warning.c_str());
        }
        node_.sendControlFrame(result.frame, "SCU_Control_Command");
      });
  }

  if (node_.isSubscribeTopicEnabled("control_scu_chassis_command")) {
    node_.scu_chassis_sub_ = node_.create_subscription<chassis_interfaces::msg::ScuChassisCommand>(
      node_.makeTopicName("control/scu_chassis_command"), qos,
      [this](const chassis_interfaces::msg::ScuChassisCommand::SharedPtr msg) {
      CanFrame frame;
      frame.can_id = 294U;
      frame.dlc = 8;
      DbcProtocol::encodeSignal(frame, "VCU_Target_Steering_Angle_Speed", msg->vcu_target_steering_angle_speed);
      DbcProtocol::encodeSignal(frame, "Brake_Force_Front_Left", msg->brake_force_front_left);
      DbcProtocol::encodeSignal(frame, "Brake_Force_Front_Right", msg->brake_force_front_right);
      DbcProtocol::encodeSignal(frame, "Brake_Force_Rear_Left", msg->brake_force_rear_left);
      DbcProtocol::encodeSignal(frame, "Brake_Force_Rear_Right", msg->brake_force_rear_right);
        node_.sendControlFrame(frame, "SCU_Chassis_Command");
      });
  }

  if (node_.isSubscribeTopicEnabled("control_scu_torque_command")) {
    node_.scu_torque_sub_ = node_.create_subscription<chassis_interfaces::msg::ScuTorqueCommand>(
      node_.makeTopicName("control/scu_torque_command"), qos,
      [this](const chassis_interfaces::msg::ScuTorqueCommand::SharedPtr msg) {
      CanFrame frame;
      frame.can_id = 291U;
      frame.dlc = 8;
      DbcProtocol::encodeSignal(frame, "Torque_Command_Front_Left", msg->torque_command_front_left);
      DbcProtocol::encodeSignal(frame, "Torque_Command_Front_Right", msg->torque_command_front_right);
      DbcProtocol::encodeSignal(frame, "Torque_Command_Rear_Left", msg->torque_command_rear_left);
      DbcProtocol::encodeSignal(frame, "Torque_Command_Rear_Right", msg->torque_command_rear_right);
        node_.sendControlFrame(frame, "SCU_Torque_Command");
      });
  }

  if (node_.isSubscribeTopicEnabled("control_vcu_chassis_debug")) {
    node_.vcu_chassis_debug_sub_ = node_.create_subscription<chassis_interfaces::msg::VcuChassisDebug>(
      node_.makeTopicName("control/vcu_chassis_debug"), qos,
      [this](const chassis_interfaces::msg::VcuChassisDebug::SharedPtr msg) {
      CanFrame enable_frame;
      enable_frame.can_id = 1808U;
      enable_frame.dlc = 8;
      DbcProtocol::encodeSignal(enable_frame, "PID_Debug_Enable", msg->pid_debug_enable ? 1.0 : 0.0);
      node_.sendControlFrame(enable_frame, "VCU_Debug_Enable");

      CanFrame debug_frame;
      debug_frame.can_id = 1813U;
      debug_frame.dlc = 8;
      DbcProtocol::encodeSignal(debug_frame, "Velocity_Kp", msg->velocity_kp);
      DbcProtocol::encodeSignal(debug_frame, "Velocity_Ki", msg->velocity_ki);
      DbcProtocol::encodeSignal(debug_frame, "Velocity_Kd", msg->velocity_kd);
      node_.sendControlFrame(debug_frame, "VCU_Drive_Debug");
      });
  }
}

}  // namespace chassis_driver
