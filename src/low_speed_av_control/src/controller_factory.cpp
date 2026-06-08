#include "low_speed_av_control/controller_factory.hpp"

#include <stdexcept>

#include "low_speed_av_control/lqr_controller.hpp"
#include "low_speed_av_control/mpc_sampler_controller.hpp"
#include "low_speed_av_control/pure_pursuit_controller.hpp"
#include "low_speed_av_control/stanley_controller.hpp"

namespace low_speed_av_control {

std::unique_ptr<ControllerBase> ControllerFactory::create(const std::string & algorithm)
{
  if (algorithm == "pure_pursuit") {
    return std::make_unique<PurePursuitController>();
  }
  if (algorithm == "stanley") {
    return std::make_unique<StanleyController>();
  }
  if (algorithm == "lqr") {
    return std::make_unique<LqrController>();
  }
  if (algorithm == "mpc_sampler") {
    return std::make_unique<MpcSamplerController>();
  }
  throw std::invalid_argument("unknown controller: " + algorithm);
}

}  // namespace low_speed_av_control
