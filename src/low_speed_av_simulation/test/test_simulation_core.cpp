#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "low_speed_av_simulation/kinematic_vehicle_plant.hpp"
#include "low_speed_av_simulation/simulation_runtime_monitor.hpp"

namespace low_speed_av_simulation {
namespace {

PlantOptions options_without_jerk() {
  PlantOptions options;
  options.max_jerk_mps3 = 0.0;
  options.max_dt_s = 0.2;
  return options;
}

PlantCommand drive_command(double speed = 1.0) {
  PlantCommand command;
  command.target_speed_mps = speed;
  command.gear = 1;
  command.enable = true;
  return command;
}

TEST(SimulationPlantProduction, StraightDrive) {
  KinematicVehiclePlant plant(options_without_jerk());
  for (int i = 0; i < 20; ++i) {
    plant.step(drive_command(), 0.1);
  }
  EXPECT_GT(plant.state().x_m, 0.5);
  EXPECT_NEAR(plant.state().y_m, 0.0, 1e-12);
  EXPECT_NEAR(plant.state().yaw_rad, 0.0, 1e-12);
}

TEST(SimulationPlantProduction, ConstantCurvatureUsesMidpointIntegration) {
  auto command = drive_command();
  command.front_steering_angle_rad = 0.3;
  KinematicVehiclePlant plant(options_without_jerk());
  for (int i = 0; i < 20; ++i) {
    plant.step(command, 0.1);
  }
  EXPECT_GT(plant.state().yaw_rad, 0.1);
  EXPECT_GT(plant.state().y_m, 0.0);
}

TEST(SimulationPlantProduction, FrontAckermannKeepsRearAtZero) {
  auto command = drive_command();
  command.front_steering_angle_rad = 0.2;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.step(command, 0.1);
  EXPECT_GT(plant.state().front_steering_angle_rad, 0.0);
  EXPECT_DOUBLE_EQ(plant.state().rear_steering_angle_rad, 0.0);
}

TEST(SimulationPlantProduction, DualAckermannUsesBothSteeringCommands) {
  auto front_only = drive_command();
  front_only.front_steering_angle_rad = 0.2;
  auto dual = front_only;
  dual.rear_steering_angle_rad = -0.1;
  KinematicVehiclePlant front_plant(options_without_jerk());
  KinematicVehiclePlant dual_plant(options_without_jerk());
  for (int i = 0; i < 20; ++i) {
    front_plant.step(front_only, 0.1);
    dual_plant.step(dual, 0.1);
  }
  EXPECT_GT(dual_plant.state().yaw_rad, front_plant.state().yaw_rad);
}

TEST(SimulationPlantProduction, AccelerationIsLimited) {
  KinematicVehiclePlant plant(options_without_jerk());
  plant.step(drive_command(), 0.1);
  EXPECT_NEAR(plant.state().speed_mps, 0.05, 1e-12);
  EXPECT_NEAR(plant.state().acceleration_mps2, 0.5, 1e-12);
}

TEST(SimulationPlantProduction, DecelerationIsLimited) {
  PlantState state;
  state.speed_mps = 1.0;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  plant.step(drive_command(0.0), 0.1);
  EXPECT_NEAR(plant.state().speed_mps, 0.92, 1e-12);
}

TEST(SimulationPlantProduction, JerkIsLimited) {
  PlantOptions options = options_without_jerk();
  options.max_jerk_mps3 = 1.0;
  KinematicVehiclePlant plant(options);
  plant.step(drive_command(), 0.1);
  EXPECT_NEAR(plant.state().acceleration_mps2, 0.1, 1e-12);
}

TEST(SimulationPlantProduction, FrontSteeringRateIsLimited) {
  PlantOptions options = options_without_jerk();
  options.max_front_steering_rate_radps = 0.5;
  auto command = drive_command();
  command.front_steering_angle_rad = 0.5;
  KinematicVehiclePlant plant(options);
  plant.step(command, 0.1);
  EXPECT_NEAR(plant.state().front_steering_angle_rad, 0.05, 1e-12);
}

TEST(SimulationPlantProduction, RearSteeringRateIsLimited) {
  PlantOptions options = options_without_jerk();
  options.max_rear_steering_rate_radps = 0.4;
  auto command = drive_command();
  command.rear_steering_angle_rad = -0.5;
  KinematicVehiclePlant plant(options);
  plant.step(command, 0.1);
  EXPECT_NEAR(plant.state().rear_steering_angle_rad, -0.04, 1e-12);
}

TEST(SimulationPlantProduction, NormalBrakeUsesBrakeDeceleration) {
  PlantState state;
  state.speed_mps = 1.0;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  auto command = drive_command();
  command.brake = 1.0;
  const auto result = plant.step(command, 0.1);
  EXPECT_EQ(result.stop_reason, "brake");
  EXPECT_NEAR(plant.state().speed_mps, 0.9, 1e-12);
}

TEST(SimulationPlantProduction, EmergencyStopUsesEmergencyDeceleration) {
  PlantState state;
  state.speed_mps = 1.0;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  auto command = drive_command();
  command.emergency_stop = true;
  const auto result = plant.step(command, 0.1);
  EXPECT_EQ(result.stop_reason, "emergency_stop");
  EXPECT_NEAR(plant.state().speed_mps, 0.85, 1e-12);
}

TEST(SimulationPlantProduction, DisabledCommandStops) {
  PlantState state;
  state.speed_mps = 0.5;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  auto command = drive_command();
  command.enable = false;
  EXPECT_EQ(plant.step(command, 0.1).stop_reason, "disabled");
  EXPECT_LT(plant.state().speed_mps, 0.5);
}

TEST(SimulationPlantProduction, InvalidDtFailsClosed) {
  PlantState state;
  state.speed_mps = 0.5;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  EXPECT_FALSE(plant.step(drive_command(), 1.0).command_valid);
  EXPECT_DOUBLE_EQ(plant.state().speed_mps, 0.0);
}

TEST(SimulationPlantProduction, InvalidWheelbaseIsRejected) {
  PlantOptions options;
  options.wheel_base_m = 0.0;
  EXPECT_THROW(KinematicVehiclePlant plant(options), std::invalid_argument);
}

TEST(SimulationPlantProduction, NonFiniteCommandFailsClosed) {
  auto command = drive_command();
  command.target_speed_mps = std::numeric_limits<double>::quiet_NaN();
  KinematicVehiclePlant plant(options_without_jerk());
  EXPECT_EQ(plant.step(command, 0.1).stop_reason, "invalid_command");
  EXPECT_DOUBLE_EQ(plant.state().speed_mps, 0.0);
}

TEST(SimulationPlantProduction, UnsupportedGearFailsClosed) {
  auto command = drive_command();
  command.gear = 3;
  KinematicVehiclePlant plant(options_without_jerk());
  EXPECT_EQ(plant.step(command, 0.1).stop_reason, "unsupported_gear");
}

TEST(SimulationPlantProduction, ReverseFailsClosed) {
  auto command = drive_command();
  command.gear = 2;
  KinematicVehiclePlant plant(options_without_jerk());
  EXPECT_EQ(plant.step(command, 0.1).stop_reason, "reverse_unsupported");
  EXPECT_GE(plant.state().speed_mps, 0.0);
}

TEST(SimulationPlantProduction, CommandTimeoutStops) {
  PlantState state;
  state.speed_mps = 0.5;
  KinematicVehiclePlant plant(options_without_jerk());
  plant.reset(state);
  auto command = drive_command();
  command.command_timed_out = true;
  EXPECT_EQ(plant.step(command, 0.1).stop_reason, "command_timeout");
  EXPECT_LT(plant.state().speed_mps, 0.5);
}

TEST(SimulationPlantProduction, ResetRestoresExactState) {
  KinematicVehiclePlant plant(options_without_jerk());
  plant.step(drive_command(), 0.1);
  PlantState reset;
  reset.x_m = 2.0;
  reset.y_m = -1.0;
  reset.yaw_rad = 0.4;
  plant.reset(reset);
  EXPECT_DOUBLE_EQ(plant.state().x_m, 2.0);
  EXPECT_DOUBLE_EQ(plant.state().speed_mps, 0.0);
}

TEST(SimulationPlantProduction, MultiStepIsDeterministic) {
  KinematicVehiclePlant first(options_without_jerk());
  KinematicVehiclePlant second(options_without_jerk());
  auto command = drive_command(0.7);
  command.front_steering_angle_rad = 0.15;
  for (int i = 0; i < 100; ++i) {
    first.step(command, 0.01);
    second.step(command, 0.01);
  }
  EXPECT_DOUBLE_EQ(first.state().x_m, second.state().x_m);
  EXPECT_DOUBLE_EQ(first.state().y_m, second.state().y_m);
  EXPECT_DOUBLE_EQ(first.state().yaw_rad, second.state().yaw_rad);
}

TEST(SimulationPlantProduction, GoalPositionToleranceIsApplied) {
  KinematicVehiclePlant plant(options_without_jerk());
  EXPECT_TRUE(plant.goal_reached(0.2, 0.0, 0.0));
  EXPECT_FALSE(plant.goal_reached(0.31, 0.0, 0.0));
}

TEST(SimulationPlantProduction, GoalYawToleranceIsApplied) {
  KinematicVehiclePlant plant(options_without_jerk());
  EXPECT_TRUE(plant.goal_reached(0.0, 0.0, 0.3));
  EXPECT_FALSE(plant.goal_reached(0.0, 0.0, 0.36));
}

TEST(SimulationMonitorProduction, ExplicitIntervalsProduceMaxAndP95) {
  SimulationRuntimeMonitor monitor(8U);
  for (double sample : {0.01, 0.02, 0.03, 0.04}) {
    monitor.observe_step_interval(sample);
    monitor.observe_localization_interval(sample * 2.0);
    monitor.observe_control_interval(sample);
  }
  const auto metrics = monitor.metrics();
  EXPECT_DOUBLE_EQ(metrics.step_interval_max_s, 0.04);
  EXPECT_DOUBLE_EQ(metrics.step_interval_p95_s, 0.04);
  EXPECT_DOUBLE_EQ(metrics.localization_interval_max_s, 0.08);
}

TEST(SimulationMonitorProduction, TrackingMetricsAreFiniteAndResettable) {
  SimulationRuntimeMonitor monitor;
  monitor.observe_tracking(-0.2, -0.1, 0.4, 0.2);
  monitor.record_timeout();
  auto metrics = monitor.metrics();
  EXPECT_DOUBLE_EQ(metrics.lateral_error_max_m, 0.2);
  EXPECT_DOUBLE_EQ(metrics.heading_error_max_rad, 0.1);
  EXPECT_EQ(metrics.timeout_count, 1U);
  monitor.reset();
  metrics = monitor.metrics();
  EXPECT_EQ(metrics.timeout_count, 0U);
  EXPECT_DOUBLE_EQ(metrics.lateral_error_max_m, 0.0);
}

TEST(SimulationMonitorProduction, NonFiniteSamplesAreCounted) {
  SimulationRuntimeMonitor monitor;
  monitor.observe_step_interval(std::numeric_limits<double>::infinity());
  monitor.observe_tracking(std::numeric_limits<double>::quiet_NaN(), 0.0,
                           std::numeric_limits<double>::quiet_NaN(), 0.0);
  EXPECT_GE(monitor.metrics().non_finite_count, 2U);
}

} // namespace
} // namespace low_speed_av_simulation
