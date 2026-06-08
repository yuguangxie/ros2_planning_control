#pragma once

#include <memory>
#include <string>

#include "low_speed_av_planning/roadnet_types.hpp"

namespace low_speed_av_planning {

struct SpeedPlannerOptions {
  // Conservative default speed for low-speed demo operation.
  double default_speed_mps{0.5};
  double max_speed_mps{1.0};
  double max_lateral_accel_mps2{0.5};
  double obstacle_distance_m{-1.0};
  double obstacle_stop_distance_m{2.0};
};

class SpeedPlannerBase {
public:
  virtual ~SpeedPlannerBase() = default;
  virtual std::string name() const = 0;
  // Modify target_speed_mps in-place; geometry and route_s_m stay unchanged.
  virtual void apply(Trajectory & trajectory, const SpeedPlannerOptions & options) const = 0;
};

class SpeedPlannerFactory {
public:
  static std::unique_ptr<SpeedPlannerBase> create(const std::string & algorithm);
};

}  // namespace low_speed_av_planning
