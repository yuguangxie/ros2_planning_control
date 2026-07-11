#include "chassis_driver/scu_control_frame_builder.hpp"

#include "chassis_driver/dbc_protocol.hpp"

#include <cmath>

namespace chassis_driver {
namespace {

constexpr uint8_t kShiftD = 1U;
constexpr uint8_t kShiftN = 2U;
constexpr uint8_t kShiftR = 3U;
constexpr uint8_t kDriveAuto = 1U;
constexpr double kSteeringRawAtMaxAngle = 120.0;

bool valid_shift(uint8_t value) {
  return value == kShiftD || value == kShiftN || value == kShiftR;
}

double steering_angle_deg_to_raw(double angle_deg, double max_angle_deg) {
  const int raw_signed = static_cast<int>(
      std::llround(angle_deg / max_angle_deg * kSteeringRawAtMaxAngle));
  return static_cast<double>(raw_signed < 0 ? 256 + raw_signed : raw_signed);
}

} // namespace

ScuControlFrameResult
build_scu_control_frame(const ScuControlFrameInput &input,
                        const ScuControlFrameLimits &limits) {
  ScuControlFrameResult result;
  if (!valid_shift(input.shift_level)) {
    result.rejection_reason = "invalid_shift";
    return result;
  }
  if (!std::isfinite(limits.max_steering_angle_deg) ||
      limits.max_steering_angle_deg <= 0.0 ||
      !std::isfinite(limits.max_target_speed_kmh) ||
      limits.max_target_speed_kmh <= 0.0) {
    result.rejection_reason = "invalid_limits";
    return result;
  }

  double speed_kmh = input.target_speed_kmh;
  if (!std::isfinite(speed_kmh) || speed_kmh < 0.0 ||
      speed_kmh > limits.max_target_speed_kmh) {
    speed_kmh = 0.0;
    result.warnings.push_back("target_speed_out_of_range_mapped_to_zero");
  }
  double front_angle = input.front_steering_angle_deg;
  if (!std::isfinite(front_angle) ||
      std::abs(front_angle) > limits.max_steering_angle_deg) {
    front_angle = 0.0;
    result.warnings.push_back("front_steering_out_of_range_mapped_to_zero");
  }
  double rear_angle = input.rear_steering_angle_deg;
  if (!std::isfinite(rear_angle) ||
      std::abs(rear_angle) > limits.max_steering_angle_deg) {
    rear_angle = 0.0;
    result.warnings.push_back("rear_steering_out_of_range_mapped_to_zero");
  }

  result.frame.can_id = 289U;
  result.frame.dlc = 8U;
  DbcProtocol::encodeSignal(result.frame, "SCU_Shift_Level_Request",
                            input.shift_level);
  DbcProtocol::encodeSignal(result.frame, "SCU_Drive_Mode_Request", kDriveAuto);
  DbcProtocol::encodeSignal(
      result.frame, "SCU_Steering_Angle_Front",
      steering_angle_deg_to_raw(front_angle, limits.max_steering_angle_deg));
  DbcProtocol::encodeSignal(
      result.frame, "SCU_Steering_Angle_Rear",
      steering_angle_deg_to_raw(rear_angle, limits.max_steering_angle_deg));
  DbcProtocol::encodeSignal(result.frame, "SCU_Target_Speed", speed_kmh);
  DbcProtocol::encodeSignal(result.frame, "SCU_Brake_Enable",
                            input.brake_enable ? 1.0 : 0.0);
  DbcProtocol::encodeSignal(result.frame, "GW_Left_Turn_Light_Request",
                            input.left_turn_light_request);
  DbcProtocol::encodeSignal(result.frame, "GW_Right_Turn_Light_Request",
                            input.right_turn_light_request);
  DbcProtocol::encodeSignal(result.frame, "GW_Position_Light_Request",
                            input.position_light_request);
  DbcProtocol::encodeSignal(result.frame, "GW_Low_Beam_Request",
                            input.low_beam_request);
  DbcProtocol::encodeSignal(result.frame, "SCU_Torque_Or_Speed_Mode",
                            input.torque_or_speed_mode);
  DbcProtocol::encodeSignal(result.frame, "Steering_Angle_Speed_Valid",
                            input.steering_angle_speed_valid ? 1.0 : 0.0);
  DbcProtocol::encodeSignal(result.frame, "Brake_Force_Command_Valid",
                            input.brake_force_command_valid ? 1.0 : 0.0);
  result.valid = true;
  return result;
}

} // namespace chassis_driver
