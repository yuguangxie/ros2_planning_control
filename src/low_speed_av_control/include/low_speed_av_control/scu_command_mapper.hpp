#pragma once

#include <string>
#include <vector>

#include <chassis_interfaces/msg/scu_control_command.hpp>

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

struct ScuMappingResult {
  chassis_interfaces::msg::ScuControlCommand command;
  std::vector<std::string> warnings;
  bool safe_stop{false};
};

class ScuCommandMapper {
public:
  ScuMappingResult map(const ControlCommand & command, const ScuCommandOptions & options) const;
  chassis_interfaces::msg::ScuControlCommand stop_command(const ScuCommandOptions & options) const;

private:
  static uint8_t sanitized_stop_shift(const ScuCommandOptions & options);
  static bool valid_shift(uint8_t shift);
};

}  // namespace low_speed_av_control
