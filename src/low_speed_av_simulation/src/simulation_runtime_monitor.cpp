#include "low_speed_av_simulation/simulation_runtime_monitor.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace low_speed_av_simulation {

SimulationRuntimeMonitor::SimulationRuntimeMonitor(std::size_t window_size)
    : window_size_(window_size) {
  if (window_size_ == 0U) {
    throw std::invalid_argument("monitor window_size must be > 0");
  }
}

void SimulationRuntimeMonitor::reset() {
  step_intervals_ = Series{};
  localization_intervals_ = Series{};
  control_intervals_ = Series{};
  lateral_errors_ = Series{};
  heading_errors_ = Series{};
  counters_ = RuntimeMetrics{};
}

void SimulationRuntimeMonitor::observe_step_interval(double interval_s) {
  observe(step_intervals_, interval_s, false);
  ++counters_.step_count;
}

void SimulationRuntimeMonitor::observe_localization_interval(
    double interval_s) {
  observe(localization_intervals_, interval_s, false);
  ++counters_.localization_publish_count;
}

void SimulationRuntimeMonitor::observe_control_interval(double interval_s) {
  observe(control_intervals_, interval_s, false);
  ++counters_.control_command_count;
}

void SimulationRuntimeMonitor::observe_tracking(double lateral_error_m,
                                                double heading_error_rad,
                                                double goal_distance_m,
                                                double goal_yaw_error_rad) {
  observe(lateral_errors_, lateral_error_m, true);
  observe(heading_errors_, heading_error_rad, true);
  if (std::isfinite(goal_distance_m)) {
    counters_.goal_distance_m = std::abs(goal_distance_m);
  } else {
    record_non_finite();
  }
  if (std::isfinite(goal_yaw_error_rad)) {
    counters_.goal_yaw_error_rad = std::abs(goal_yaw_error_rad);
  } else {
    record_non_finite();
  }
}

void SimulationRuntimeMonitor::record_timeout() { ++counters_.timeout_count; }

void SimulationRuntimeMonitor::record_non_finite() {
  ++counters_.non_finite_count;
}

RuntimeMetrics SimulationRuntimeMonitor::metrics() const {
  RuntimeMetrics result = counters_;
  result.step_interval_max_s = step_intervals_.max;
  result.step_interval_p95_s = percentile95(step_intervals_);
  result.localization_interval_max_s = localization_intervals_.max;
  result.localization_interval_p95_s = percentile95(localization_intervals_);
  result.control_interval_max_s = control_intervals_.max;
  result.control_interval_p95_s = percentile95(control_intervals_);
  result.lateral_error_max_m = lateral_errors_.max;
  result.lateral_error_rms_m = rms(lateral_errors_);
  result.lateral_error_p95_m = percentile95(lateral_errors_);
  result.heading_error_max_rad = heading_errors_.max;
  result.heading_error_rms_rad = rms(heading_errors_);
  result.heading_error_p95_rad = percentile95(heading_errors_);
  return result;
}

void SimulationRuntimeMonitor::observe(Series &series, double value,
                                       bool use_absolute) {
  if (!std::isfinite(value) || (!use_absolute && value < 0.0)) {
    record_non_finite();
    return;
  }
  const double sample = use_absolute ? std::abs(value) : value;
  series.max = std::max(series.max, sample);
  series.sum_squares += sample * sample;
  ++series.count;
  series.values.push_back(sample);
  if (series.values.size() > window_size_) {
    series.values.pop_front();
  }
}

double SimulationRuntimeMonitor::percentile95(const Series &series) const {
  if (series.values.empty()) {
    return 0.0;
  }
  std::vector<double> sorted(series.values.begin(), series.values.end());
  std::sort(sorted.begin(), sorted.end());
  const auto index = static_cast<std::size_t>(
                         std::ceil(0.95 * static_cast<double>(sorted.size()))) -
                     1U;
  return sorted[std::min(index, sorted.size() - 1U)];
}

double SimulationRuntimeMonitor::rms(const Series &series) const {
  return series.count == 0U ? 0.0
                            : std::sqrt(series.sum_squares /
                                        static_cast<double>(series.count));
}

} // namespace low_speed_av_simulation
