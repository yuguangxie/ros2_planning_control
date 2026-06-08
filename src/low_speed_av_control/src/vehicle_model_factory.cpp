#include "low_speed_av_control/vehicle_model_factory.hpp"

#include <stdexcept>

#include "low_speed_av_control/dual_ackermann_model.hpp"
#include "low_speed_av_control/front_ackermann_model.hpp"

namespace low_speed_av_control {

std::unique_ptr<VehicleModelBase> VehicleModelFactory::create(const std::string & model)
{
  if (model == "front_ackermann") {
    return std::make_unique<FrontAckermannModel>();
  }
  if (model == "dual_ackermann") {
    return std::make_unique<DualAckermannModel>();
  }
  throw std::invalid_argument("unknown vehicle model: " + model);
}

}  // namespace low_speed_av_control
