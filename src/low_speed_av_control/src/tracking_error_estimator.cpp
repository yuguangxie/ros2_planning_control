#include "low_speed_av_control/tracking_error_estimator.hpp"

#include <cmath>
#include <limits>

namespace low_speed_av_control {

TrackingError TrackingErrorEstimator::estimate(const Pose2d & pose, const Trajectory & trajectory) const
{
  TrackingError error;
  if (trajectory.empty()) {
    return error;
  }
  double best = std::numeric_limits<double>::max();
  for (std::size_t i = 0; i < trajectory.size(); ++i) {
    const double d = std::hypot(trajectory[i].x_m - pose.x_m, trajectory[i].y_m - pose.y_m);
    if (d < best) {
      best = d;
      error.nearest_index = i;
    }
  }
  const auto & ref = trajectory[error.nearest_index];
  error.heading_error_rad = std::atan2(std::sin(ref.yaw_rad - pose.yaw_rad), std::cos(ref.yaw_rad - pose.yaw_rad));
  error.lateral_error_m = -std::sin(ref.yaw_rad) * (pose.x_m - ref.x_m) +
    std::cos(ref.yaw_rad) * (pose.y_m - ref.y_m);
  return error;
}

}  // namespace low_speed_av_control
