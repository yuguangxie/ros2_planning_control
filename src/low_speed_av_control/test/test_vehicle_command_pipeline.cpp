#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

#include "low_speed_av_control/command_limiter.hpp"
#include "low_speed_av_control/command_smoother.hpp"
#include "low_speed_av_control/scu_command_mapper.hpp"
#include "low_speed_av_control/vehicle_model_factory.hpp"

namespace low_speed_av_control {
namespace {

TEST(VehicleModelsProduction, FrontAckermannMatchesFormulaAndLimits) {
  VehicleLimits limits;
  limits.wheel_base_m = 1.2;
  limits.max_front_steer_rad = 0.3;
  const auto model = VehicleModelFactory::create("front_ackermann");
  const auto nominal = model->steering_from_curvature(0.1, limits);
  EXPECT_NEAR(nominal.front_steering_angle_rad, std::atan(0.12), 1.0e-12);
  EXPECT_DOUBLE_EQ(nominal.rear_steering_angle_rad, 0.0);
  const auto limited = model->steering_from_curvature(100.0, limits);
  EXPECT_DOUBLE_EQ(limited.front_steering_angle_rad, 0.3);
}

TEST(VehicleModelsProduction,
     DualAckermannUsesCounterPhaseAndIndependentLimits) {
  VehicleLimits limits;
  limits.wheel_base_m = 1.2;
  limits.rear_steer_ratio = 0.5;
  limits.max_front_steer_rad = 0.6;
  limits.max_rear_steer_rad = 0.2;
  const auto command = VehicleModelFactory::create("dual_ackermann")
                           ->steering_from_curvature(0.5, limits);
  EXPECT_GT(command.front_steering_angle_rad, 0.0);
  EXPECT_LT(command.rear_steering_angle_rad, 0.0);
  EXPECT_LE(std::abs(command.front_steering_angle_rad),
            limits.max_front_steer_rad);
  EXPECT_LE(std::abs(command.rear_steering_angle_rad),
            limits.max_rear_steer_rad);
  const double reconstructed = (std::tan(command.front_steering_angle_rad) -
                                std::tan(command.rear_steering_angle_rad)) /
                               limits.wheel_base_m;
  EXPECT_NEAR(reconstructed, 0.5, 1.0e-9);
}

TEST(CommandLimiterProduction, ClampsSpeedAccelerationAndBothSteeringAngles) {
  VehicleLimits limits;
  limits.max_speed_mps = 1.0;
  limits.max_accel_mps2 = 0.4;
  limits.max_decel_mps2 = 0.7;
  limits.max_front_steer_rad = 0.3;
  limits.max_rear_steer_rad = 0.2;
  ControlCommand input;
  input.speed_mps = 5.0;
  input.acceleration_mps2 = -5.0;
  input.front_steering_angle_rad = 1.0;
  input.rear_steering_angle_rad = -1.0;
  const auto output = CommandLimiter().limit(input, limits);
  EXPECT_DOUBLE_EQ(output.speed_mps, 1.0);
  EXPECT_DOUBLE_EQ(output.acceleration_mps2, -0.7);
  EXPECT_DOUBLE_EQ(output.front_steering_angle_rad, 0.3);
  EXPECT_DOUBLE_EQ(output.rear_steering_angle_rad, -0.2);
}

TEST(CommandLimiterProduction, NonFiniteCommandFailsClosed) {
  ControlCommand input;
  input.speed_mps = std::numeric_limits<double>::quiet_NaN();
  const auto output = CommandLimiter().limit(input, VehicleLimits{});
  EXPECT_DOUBLE_EQ(output.speed_mps, 0.0);
  EXPECT_FALSE(output.enable);
  EXPECT_TRUE(output.emergency_stop);
  EXPECT_DOUBLE_EQ(output.brake, 1.0);
  EXPECT_EQ(output.reason, "nan_or_inf_guard");
}

TEST(CommandSmootherProduction,
     UsesActualDtForAccelDecelAndIndependentSteeringRates) {
  CommandSmoother smoother;
  SmootherOptions options;
  options.max_accel_mps2 = 1.0;
  options.max_decel_mps2 = 2.0;
  options.max_jerk_mps3 = 100.0;
  options.max_front_steer_rate_radps = 0.5;
  options.max_rear_steer_rate_radps = 0.25;
  ControlCommand moving;
  moving.speed_mps = 1.0;
  moving.front_steering_angle_rad = 0.5;
  moving.rear_steering_angle_rad = -0.5;
  const auto first = smoother.smooth(moving, options, 0.1);
  const auto second = smoother.smooth(moving, options, 0.1);
  EXPECT_DOUBLE_EQ(first.speed_mps, 0.1);
  EXPECT_DOUBLE_EQ(second.speed_mps, 0.2);
  EXPECT_NEAR(first.front_steering_angle_rad, 0.05, 1.0e-12);
  EXPECT_NEAR(first.rear_steering_angle_rad, -0.025, 1.0e-12);

  ControlCommand slower = moving;
  slower.speed_mps = 0.0;
  const auto held = smoother.smooth(slower, options, 0.1);
  EXPECT_LE(held.speed_mps, second.speed_mps);
  EXPECT_GE(held.acceleration_mps2, -options.max_decel_mps2);
}

TEST(CommandSmootherProduction, LimitsJerkClampsBadDtAndResetIsDeterministic) {
  CommandSmoother smoother;
  SmootherOptions options;
  options.max_accel_mps2 = 1.0;
  options.max_jerk_mps3 = 1.0;
  options.min_dt_s = 0.01;
  options.max_dt_s = 0.1;
  ControlCommand moving;
  moving.speed_mps = 1.0;
  const auto first = smoother.smooth(moving, options, 0.1);
  EXPECT_NEAR(first.acceleration_mps2, 0.1, 1.0e-12);
  EXPECT_TRUE(smoother.diagnostics().jerk_limited);
  const auto bad_dt = smoother.smooth(moving, options, 10.0);
  EXPECT_TRUE(smoother.diagnostics().dt_clamped);
  EXPECT_DOUBLE_EQ(smoother.diagnostics().applied_dt_s, 0.1);
  EXPECT_GT(bad_dt.speed_mps, first.speed_mps);
  smoother.reset();
  const auto reset_first = smoother.smooth(moving, options, 0.1);
  EXPECT_DOUBLE_EQ(reset_first.speed_mps, first.speed_mps);
}

TEST(CommandSmootherProduction, EverySafetyStopBypassesNormalSmoothing) {
  CommandSmoother smoother;
  SmootherOptions options;
  ControlCommand moving;
  moving.speed_mps = 1.0;
  moving.front_steering_angle_rad = 0.5;
  (void)smoother.smooth(moving, options, 0.02);
  ControlCommand stop;
  stop.enable = false;
  stop.brake = 1.0;
  stop.reason = "test_stop";
  const auto emergency = smoother.smooth(stop, options, 0.02);
  EXPECT_DOUBLE_EQ(emergency.speed_mps, 0.0);
  EXPECT_DOUBLE_EQ(emergency.front_steering_angle_rad, 0.0);
  EXPECT_DOUBLE_EQ(emergency.brake, 1.0);
  EXPECT_TRUE(smoother.diagnostics().safety_bypass);
}

TEST(VehicleModelsProduction, InvalidGeometryFailsFast) {
  VehicleLimits limits;
  limits.wheel_base_m = 0.0;
  EXPECT_THROW(VehicleModelFactory::create("front_ackermann")
                   ->steering_from_curvature(0.1, limits),
               std::invalid_argument);
  limits.wheel_base_m = 1.2;
  limits.rear_steer_ratio = -0.1;
  EXPECT_THROW(VehicleModelFactory::create("dual_ackermann")
                   ->steering_from_curvature(0.1, limits),
               std::invalid_argument);
}

TEST(ScuCommandMapperProduction, ConvertsUnitsSignsAndDriveReverseNeutral) {
  ScuCommandMapper mapper;
  ScuCommandOptions options;
  options.front_steer_sign = -1.0;
  options.rear_steer_sign = 1.0;
  ControlCommand command;
  command.speed_mps = 1.0;
  command.front_steering_angle_rad = 0.1;
  command.rear_steering_angle_rad = -0.05;
  command.gear = 1;
  auto mapped = mapper.map(command, options);
  EXPECT_FALSE(mapped.safe_stop);
  EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 3.6F);
  EXPECT_LT(mapped.command.scu_steering_angle_front, 0.0F);
  EXPECT_LT(mapped.command.scu_steering_angle_rear, 0.0F);
  EXPECT_EQ(mapped.command.scu_shift_level_request, 1U);
  command.gear = 2;
  EXPECT_EQ(mapper.map(command, options).command.scu_shift_level_request, 3U);
  command.gear = 4;
  EXPECT_EQ(mapper.map(command, options).command.scu_shift_level_request, 2U);
}

TEST(ScuCommandMapperProduction, ParkUnknownAndSafetyInputsMapToFixedStop) {
  ScuCommandMapper mapper;
  ScuCommandOptions options;
  options.stop_shift_level = 2U;
  for (const int gear : {0, 3, 99}) {
    ControlCommand command;
    command.gear = gear;
    const auto mapped = mapper.map(command, options);
    EXPECT_TRUE(mapped.safe_stop);
    EXPECT_EQ(mapped.command.scu_shift_level_request, 2U);
    EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 0.0F);
    EXPECT_TRUE(mapped.command.scu_brake_enable);
  }
  for (const auto mode : {0, 1, 2}) {
    ControlCommand command;
    command.gear = 1;
    command.emergency_stop = mode == 0;
    command.enable = mode != 1;
    command.brake = mode == 2 ? 1.0 : 0.0;
    EXPECT_TRUE(mapper.map(command, options).safe_stop);
  }
}

TEST(ScuCommandMapperProduction,
     ClampZeroAndNonFinitePoliciesAreDeterministic) {
  ScuCommandMapper mapper;
  ScuCommandOptions options;
  options.max_target_speed_kmh = 5.0;
  options.max_steering_angle_deg = 10.0;
  ControlCommand command;
  command.gear = 1;
  command.speed_mps = 10.0;
  command.front_steering_angle_rad = 1.0;
  options.overrange_policy = "clamp";
  auto mapped = mapper.map(command, options);
  EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 5.0F);
  EXPECT_FLOAT_EQ(mapped.command.scu_steering_angle_front, 10.0F);
  options.overrange_policy = "zero";
  mapped = mapper.map(command, options);
  EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 0.0F);
  EXPECT_FLOAT_EQ(mapped.command.scu_steering_angle_front, 0.0F);
  command.speed_mps = std::numeric_limits<double>::quiet_NaN();
  mapped = mapper.map(command, options);
  EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 0.0F);
}

} // namespace
} // namespace low_speed_av_control
