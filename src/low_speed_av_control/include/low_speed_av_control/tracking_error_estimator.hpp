#pragma once

#include "low_speed_av_control/control_types.hpp"

namespace low_speed_av_control {

struct TrackingError {
  double lateral_error_m{0.0};
  double heading_error_rad{0.0};
  std::size_t nearest_index{0};
};

class TrackingErrorEstimator {
public:
  TrackingError estimate(const Pose2d & pose, const Trajectory & trajectory) const;
};

}  // namespace low_speed_av_control
