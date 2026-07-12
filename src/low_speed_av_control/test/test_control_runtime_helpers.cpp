#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

#include "low_speed_av_control/control_runtime_helpers.hpp"
#include "low_speed_av_control/controller_factory.hpp"

namespace low_speed_av_control {
namespace {

ControlConfiguration valid_configuration() {
  ControlConfiguration config;
  config.controller_options.control_dt_s = 0.02;
  config.smoother_options.max_front_steer_rate_radps = 0.5;
  config.smoother_options.max_rear_steer_rate_radps = 0.5;
  return config;
}

Trajectory loop_trajectory() {
  Trajectory trajectory;
  for (std::size_t i = 0U; i < 10U; ++i) {
    TrajectoryPoint point;
    point.x_m = i < 5U ? static_cast<double>(i) : static_cast<double>(9U - i);
    point.y_m = i < 5U ? 0.0 : 0.05;
    point.yaw_rad = i < 5U ? 0.0 : 3.14159265358979323846;
    point.s_m = static_cast<double>(i);
    point.v_mps = 0.3;
    point.gear = 1;
    trajectory.push_back(point);
  }
  return trajectory;
}

TEST(ControlConfigurationProduction,
     ValidDefaultsPassAndSafetyParametersFailFast) {
  EXPECT_NO_THROW(validate_control_configuration(valid_configuration()));

  auto config = valid_configuration();
  config.output_mode = "unknown";
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.vehicle_limits.wheel_base_m = 0.0;
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.controller_options.lqr_r_steering = 0.0;
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.smoother_options.max_jerk_mps3 =
      std::numeric_limits<double>::quiet_NaN();
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.timing.publish_deadline_warning_s = 0.5;
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.timing.hardware_watchdog_timeout_s = 0.6;
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.scu_options.stop_shift_level = 9U;
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
  config = valid_configuration();
  config.allowed_trajectory_statuses = {"failure"};
  EXPECT_THROW(validate_control_configuration(config), std::invalid_argument);
}

TEST(ControlInputProduction, InvalidReverseAndStopInputsFailClosed) {
  const auto options = ControllerOptions{};
  auto trajectory = loop_trajectory();
  EXPECT_TRUE(
      validate_controller_input(Pose2d{}, VehicleState{}, trajectory, options)
          .valid);
  auto vehicle_state = VehicleState{};
  vehicle_state.gear = 2;
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, vehicle_state, trajectory, options)
          .reason,
      "unsupported_reverse_vehicle_state");
  vehicle_state.gear = 3;
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, vehicle_state, trajectory, options)
          .reason,
      "vehicle_state_not_in_drive");
  vehicle_state = VehicleState{};
  vehicle_state.speed_mps = -0.1;
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, vehicle_state, trajectory, options)
          .reason,
      "unexpected_negative_vehicle_speed");
  trajectory.front().gear = 2;
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, VehicleState{}, trajectory, options)
          .reason,
      "unsupported_reverse_tracking");
  trajectory.front().gear = 1;
  trajectory.front().v_mps = -0.1;
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, VehicleState{}, trajectory, options)
          .reason,
      "negative_controller_trajectory_speed:0");
  for (auto &point : trajectory) {
    point.gear = 1;
    point.v_mps = 0.0;
  }
  EXPECT_EQ(
      validate_controller_input(Pose2d{}, VehicleState{}, trajectory, options)
          .reason,
      "stop_trajectory");
}

TEST(ControlProgressProduction,
     LoopSearchIsWindowedMonotonicAndIdentityResettable) {
  TrackingProgressTracker tracker;
  TrackingProgressOptions options;
  options.backward_window_points = 1U;
  options.forward_window_points = 3U;
  options.max_heading_error_rad = 0.5;
  const auto trajectory = loop_trajectory();
  auto selected = tracker.select_window(trajectory, Pose2d{0.1, 0.04, 0.0}, 1,
                                        "route-a", options);
  ASSERT_FALSE(selected.empty());
  EXPECT_LE(selected.size(), options.forward_window_points + 1U);
  EXPECT_LT(tracker.progress_index(), 5U);
  selected = tracker.select_window(trajectory, Pose2d{3.1, 0.0, 0.0}, 1,
                                   "route-a", options);
  EXPECT_GE(tracker.progress_index(), 3U);
  const auto previous = tracker.progress_index();
  (void)tracker.select_window(trajectory,
                              Pose2d{0.1, 0.05, 3.14159265358979323846}, 1,
                              "route-a", options);
  EXPECT_GE(tracker.progress_index(), previous);
  (void)tracker.select_window(trajectory, Pose2d{0.0, 0.0, 0.0}, 1, "route-b",
                              options);
  EXPECT_EQ(tracker.progress_index(), 0U);
  tracker.reset();
  EXPECT_EQ(tracker.progress_index(), 0U);
}

TEST(ControlProgressProduction,
     WindowedTrajectoryFeedsEveryControllerDeterministically) {
  TrackingProgressTracker tracker;
  TrackingProgressOptions progress;
  progress.forward_window_points = 4U;
  const auto selected =
      tracker.select_window(loop_trajectory(), Pose2d{1.1, 0.0, 0.0}, 1,
                            "controller-matrix", progress);
  ASSERT_FALSE(selected.empty());
  VehicleState state;
  state.speed_mps = 0.2;
  for (const auto &algorithm :
       {"pure_pursuit", "stanley", "lqr", "mpc_sampler"}) {
    const auto controller = ControllerFactory::create(algorithm);
    const auto first = controller->compute(Pose2d{1.1, 0.0, 0.0}, state,
                                           selected, ControllerOptions{});
    const auto second = controller->compute(Pose2d{1.1, 0.0, 0.0}, state,
                                            selected, ControllerOptions{});
    ASSERT_TRUE(first.enable) << algorithm;
    EXPECT_TRUE(std::isfinite(first.desired_curvature_1pm)) << algorithm;
    EXPECT_DOUBLE_EQ(first.desired_curvature_1pm, second.desired_curvature_1pm)
        << algorithm;
  }
}

TEST(ControlCadenceProduction, FakeClockTracksP95MissesDeadlineAndHardwareGap) {
  ControlTimingOptions options;
  options.control_rate_hz = 50.0;
  options.publish_deadline_warning_s = 0.1;
  options.hardware_watchdog_timeout_s = 0.5;
  options.cadence_window_size = 16U;
  ControlCadenceMonitor monitor(options);
  EXPECT_DOUBLE_EQ(monitor.observe_cycle(0.0), 0.02);
  EXPECT_NEAR(monitor.observe_cycle(0.02), 0.02, 1.0e-12);
  EXPECT_NEAR(monitor.observe_cycle(0.04), 0.02, 1.0e-12);
  EXPECT_NEAR(monitor.observe_cycle(0.20), 0.16, 1.0e-12);
  auto snapshot = monitor.snapshot(0.20);
  EXPECT_GT(snapshot.missed_cycles, 0U);
  EXPECT_EQ(snapshot.deadline_misses, 1U);
  EXPECT_FALSE(snapshot.hardware_timeout_gap);
  EXPECT_NEAR(monitor.observe_cycle(0.80), 0.60, 1.0e-12);
  monitor.mark_publish(0.80);
  snapshot = monitor.snapshot(0.81);
  EXPECT_TRUE(snapshot.hardware_timeout_gap);
  EXPECT_NEAR(snapshot.last_publish_age_s, 0.01, 1.0e-12);
  EXPECT_GE(snapshot.p95_interval_s, 0.16);
}

} // namespace
} // namespace low_speed_av_control
