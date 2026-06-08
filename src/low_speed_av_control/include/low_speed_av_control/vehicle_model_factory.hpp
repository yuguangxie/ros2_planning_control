#pragma once

#include <memory>
#include <string>

#include "low_speed_av_control/vehicle_model_base.hpp"

namespace low_speed_av_control {

class VehicleModelFactory {
public:
  static std::unique_ptr<VehicleModelBase> create(const std::string & model);
};

}  // namespace low_speed_av_control
