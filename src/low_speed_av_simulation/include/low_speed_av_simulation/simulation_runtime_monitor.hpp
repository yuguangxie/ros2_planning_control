#pragma once

#include <cstddef>
#include <deque>

#include "low_speed_av_simulation/simulation_types.hpp"

namespace low_speed_av_simulation {

class SimulationRuntimeMonitor {
public:
  explicit SimulationRuntimeMonitor(std::size_t window_size = 256U);

  void reset();
  void observe_step_interval(double interval_s);
  void observe_localization_interval(double interval_s);
  void observe_control_interval(double interval_s);
  void observe_tracking(double lateral_error_m, double heading_error_rad,
                        double goal_distance_m, double goal_yaw_error_rad);
  void record_timeout();
  void record_non_finite();
  RuntimeMetrics metrics() const;

private:
  struct Series {
    std::deque<double> values;
    double max{0.0};
    double sum_squares{0.0};
    std::size_t count{0U};
  };

  void observe(Series &series, double value, bool use_absolute);
  double percentile95(const Series &series) const;
  double rms(const Series &series) const;

  std::size_t window_size_;
  Series step_intervals_;
  Series localization_intervals_;
  Series control_intervals_;
  Series lateral_errors_;
  Series heading_errors_;
  RuntimeMetrics counters_;
};

} // namespace low_speed_av_simulation
