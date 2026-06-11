#include "low_speed_av_control/scu_command_mapper.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace low_speed_av_control {
namespace {

constexpr uint8_t kShiftDrive = 1;
constexpr uint8_t kShiftNeutral = 2;
constexpr uint8_t kShiftReverse = 3;
constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;

chassis_interfaces::msg::ScuControlCommand default_command(const ScuCommandOptions & options)
{
  chassis_interfaces::msg::ScuControlCommand msg;
  msg.gw_left_turn_light_request = options.left_turn_light;
  msg.gw_right_turn_light_request = options.right_turn_light;
  msg.gw_position_light_request = options.position_light;
  msg.gw_low_beam_request = options.low_beam;
  msg.scu_torque_or_speed_mode = options.torque_or_speed_mode;
  msg.steering_angle_speed_valid = options.steering_angle_speed_valid;
  msg.brake_force_command_valid = options.brake_force_command_valid;
  return msg;
}

std::string format_warning(const std::string & field, double value)
{
  std::ostringstream out;
  out << field << " invalid for SCU output, mapped to 0.0: " << value;
  return out.str();
}

std::string format_clamp_warning(const std::string & field, double value, double limit)
{
  std::ostringstream out;
  out << field << " exceeded SCU limit and was clamped: value=" << value << " limit=" << limit;
  return out.str();
}

double sanitize_steering_deg(
  double steering_rad,
  double sign,
  double max_abs_deg,
  const std::string & overrange_policy,
  const std::string & field,
  std::vector<std::string> & warnings)
{
  const double steering_deg = steering_rad * kRadToDeg * sign;
  if (!std::isfinite(steering_deg)) {
    warnings.push_back(format_warning(field, steering_deg));
    return 0.0;
  }
  if (std::abs(steering_deg) > max_abs_deg) {
    if (overrange_policy == "clamp") {
      warnings.push_back(format_clamp_warning(field, steering_deg, max_abs_deg));
      return std::clamp(steering_deg, -max_abs_deg, max_abs_deg);
    }
    warnings.push_back(format_warning(field, steering_deg));
    return 0.0;
  }
  return steering_deg;
}

double sanitize_speed_kmh(
  double speed_mps,
  double max_speed_kmh,
  const std::string & overrange_policy,
  std::vector<std::string> & warnings)
{
  const double speed_kmh = std::abs(speed_mps) * 3.6;
  if (!std::isfinite(speed_kmh)) {
    warnings.push_back(format_warning("scu_target_speed", speed_kmh));
    return 0.0;
  }
  if (speed_kmh > max_speed_kmh) {
    if (overrange_policy == "clamp") {
      warnings.push_back(format_clamp_warning("scu_target_speed", speed_kmh, max_speed_kmh));
      return max_speed_kmh;
    }
    warnings.push_back(format_warning("scu_target_speed", speed_kmh));
    return 0.0;
  }
  return speed_kmh;
}

uint8_t gear_to_shift(int gear)
{
  switch (gear) {
    case 1:
      return kShiftDrive;
    case 2:
      return kShiftReverse;
    case 4:
      return kShiftNeutral;
    default:
      return 0;
  }
}

}  // namespace

bool ScuCommandMapper::valid_shift(uint8_t shift)
{
  return shift == kShiftDrive || shift == kShiftNeutral || shift == kShiftReverse;
}

uint8_t ScuCommandMapper::sanitized_stop_shift(const ScuCommandOptions & options)
{
  return valid_shift(options.stop_shift_level) ? options.stop_shift_level : kShiftDrive;
}

chassis_interfaces::msg::ScuControlCommand ScuCommandMapper::stop_command(
  const ScuCommandOptions & options) const
{
  auto msg = default_command(options);
  msg.scu_shift_level_request = sanitized_stop_shift(options);
  msg.scu_steering_angle_front = 0.0F;
  msg.scu_steering_angle_rear = 0.0F;
  msg.scu_target_speed = 0.0F;
  msg.scu_brake_enable = true;
  msg.scu_torque_or_speed_mode = options.torque_or_speed_mode;
  msg.steering_angle_speed_valid = false;
  msg.brake_force_command_valid = false;
  msg.gw_left_turn_light_request = 0;
  msg.gw_right_turn_light_request = 0;
  msg.gw_position_light_request = 0;
  msg.gw_low_beam_request = 0;
  return msg;
}

ScuMappingResult ScuCommandMapper::map(
  const ControlCommand & command,
  const ScuCommandOptions & options) const
{
  ScuMappingResult result;
  if (command.emergency_stop || command.brake > 0.0 || !command.enable) {
    result.command = stop_command(options);
    result.safe_stop = true;
    return result;
  }

  const uint8_t shift = gear_to_shift(command.gear);
  if (!valid_shift(shift)) {
    result.warnings.push_back("internal gear is unknown for SCU output, publishing brake stop");
    result.command = stop_command(options);
    result.safe_stop = true;
    return result;
  }

  auto msg = default_command(options);
  msg.scu_shift_level_request = shift;
  msg.scu_steering_angle_front = static_cast<float>(sanitize_steering_deg(
    command.front_steering_angle_rad,
    options.front_steer_sign,
    options.max_steering_angle_deg,
    options.overrange_policy,
    "scu_steering_angle_front",
    result.warnings));
  msg.scu_steering_angle_rear = static_cast<float>(sanitize_steering_deg(
    command.rear_steering_angle_rad,
    options.rear_steer_sign,
    options.max_steering_angle_deg,
    options.overrange_policy,
    "scu_steering_angle_rear",
    result.warnings));
  msg.scu_target_speed = static_cast<float>(sanitize_speed_kmh(
    command.speed_mps,
    options.max_target_speed_kmh,
    options.overrange_policy,
    result.warnings));
  msg.scu_brake_enable = false;
  result.command = msg;
  return result;
}

}  // namespace low_speed_av_control
