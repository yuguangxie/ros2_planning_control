#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include "low_speed_av_control/controller_factory.hpp"

namespace low_speed_av_control {
namespace {

Trajectory nominal_trajectory() {
  return {{0.0, 0.0, 0.0, 0.0, 0.0, 0.5, 1},
          {1.0, 0.1, 0.05, 0.02, 1.0, 0.5, 1},
          {2.0, 0.2, 0.05, 0.02, 2.0, 0.3, 1}};
}

void expect_finite(const ControlCommand &command) {
  EXPECT_TRUE(std::isfinite(command.speed_mps));
  EXPECT_TRUE(std::isfinite(command.steering_angle_rad));
  EXPECT_TRUE(std::isfinite(command.desired_curvature_1pm));
}

TEST(ControllersProduction, AllAlgorithmsProduceFiniteDeterministicOutput) {
  const std::vector<std::string> algorithms{"pure_pursuit", "stanley", "lqr",
                                            "mpc_sampler"};
  const Pose2d pose{0.1, 0.05, 0.01};
  VehicleState state;
  state.speed_mps = 0.2;
  const auto trajectory = nominal_trajectory();
  const ControllerOptions options;
  for (const auto &algorithm : algorithms) {
    SCOPED_TRACE(algorithm);
    const auto controller = ControllerFactory::create(algorithm);
    const auto first = controller->compute(pose, state, trajectory, options);
    const auto second = controller->compute(pose, state, trajectory, options);
    expect_finite(first);
    EXPECT_DOUBLE_EQ(first.speed_mps, second.speed_mps);
    EXPECT_DOUBLE_EQ(first.steering_angle_rad, second.steering_angle_rad);
    EXPECT_DOUBLE_EQ(first.desired_curvature_1pm, second.desired_curvature_1pm);
  }
}

TEST(ControllersProduction, EmptyTrajectoryFailsClosedForEveryAlgorithm) {
  const std::vector<std::string> algorithms{"pure_pursuit", "stanley", "lqr",
                                            "mpc_sampler"};
  for (const auto &algorithm : algorithms) {
    SCOPED_TRACE(algorithm);
    const auto command = ControllerFactory::create(algorithm)->compute(
        Pose2d{}, VehicleState{}, Trajectory{}, ControllerOptions{});
    EXPECT_FALSE(command.enable);
    EXPECT_TRUE(command.brake > 0.0);
    EXPECT_EQ(command.reason, "empty_trajectory");
  }
}

TEST(ControllersProduction, SinglePointAndZeroSpeedInputsRemainBounded) {
  const std::vector<std::string> algorithms{"pure_pursuit", "stanley", "lqr",
                                            "mpc_sampler"};
  const Trajectory trajectory{{0.0, 0.0, 0.0, 0.0, 0.0, 0.2, 1}};
  VehicleState state;
  state.speed_mps = 0.0;
  for (const auto &algorithm : algorithms) {
    SCOPED_TRACE(algorithm);
    const auto command = ControllerFactory::create(algorithm)->compute(
        Pose2d{}, state, trajectory, ControllerOptions{});
    expect_finite(command);
    EXPECT_LE(std::abs(command.steering_angle_rad), 1.0);
  }
}

TEST(ControllersProduction,
     ReverseGearFailsClosedUntilDedicatedTrackingExists) {
  auto trajectory = nominal_trajectory();
  for (auto &point : trajectory) {
    point.gear = 2;
  }
  for (const auto &algorithm :
       {"pure_pursuit", "stanley", "lqr", "mpc_sampler"}) {
    const auto command = ControllerFactory::create(algorithm)->compute(
        Pose2d{}, VehicleState{}, trajectory, ControllerOptions{});
    EXPECT_FALSE(command.enable) << algorithm;
    EXPECT_EQ(command.reason, "unsupported_reverse_tracking") << algorithm;
  }
}

TEST(ControllersProduction, NonFinitePoseStateAndTrajectoryFailClosed) {
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  for (const auto &algorithm :
       {"pure_pursuit", "stanley", "lqr", "mpc_sampler"}) {
    auto controller = ControllerFactory::create(algorithm);
    auto command =
        controller->compute(Pose2d{nan, 0.0, 0.0}, VehicleState{},
                            nominal_trajectory(), ControllerOptions{});
    EXPECT_FALSE(command.enable) << algorithm;
    VehicleState state;
    state.speed_mps = nan;
    command = controller->compute(Pose2d{}, state, nominal_trajectory(),
                                  ControllerOptions{});
    EXPECT_FALSE(command.enable) << algorithm;
    auto trajectory = nominal_trajectory();
    trajectory.front().yaw_rad = nan;
    command = controller->compute(Pose2d{}, VehicleState{}, trajectory,
                                  ControllerOptions{});
    EXPECT_FALSE(command.enable) << algorithm;
  }
}

TEST(ControllersProduction, AllZeroSpeedTrajectoryRequestsControlledStop) {
  auto trajectory = nominal_trajectory();
  for (auto &point : trajectory) {
    point.v_mps = 0.0;
  }
  for (const auto &algorithm :
       {"pure_pursuit", "stanley", "lqr", "mpc_sampler"}) {
    const auto command = ControllerFactory::create(algorithm)->compute(
        Pose2d{}, VehicleState{}, trajectory, ControllerOptions{});
    EXPECT_FALSE(command.enable) << algorithm;
    EXPECT_EQ(command.reason, "stop_trajectory") << algorithm;
  }
}

} // namespace
} // namespace low_speed_av_control
