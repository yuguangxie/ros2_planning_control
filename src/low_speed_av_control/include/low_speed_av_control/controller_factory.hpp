#pragma once

#include <memory>
#include <string>

#include "low_speed_av_control/controller_base.hpp"

namespace low_speed_av_control {

class ControllerFactory {
public:
  static std::unique_ptr<ControllerBase> create(const std::string & algorithm);
};

}  // namespace low_speed_av_control
