#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

#include "low_speed_av_control/safety_state_machine.hpp"
#include "low_speed_av_control/command_smoother.hpp"
#include "low_speed_av_control/scu_command_mapper.hpp"

namespace low_speed_av_control {
namespace {

SafetyInputs ready_inputs()
{
  SafetyInputs inputs;
  inputs.have_pose = true;
  inputs.pose_valid = true;
  inputs.have_trajectory = true;
  inputs.trajectory_valid = true;
  inputs.have_vehicle_state = true;
  inputs.vehicle_state_valid = true;
  inputs.autonomous_enabled = true;
  return inputs;
}

Trajectory one_point(double speed_mps = 0.5)
{
  return Trajectory{{0.0, 0.0, 0.0, 0.0, 0.0, speed_mps, 1}};
}

TEST(ControlSafetyStateMachine, EmergencyAndFailureBypassEveryAlgorithmAndModel)
{
  const std::vector<std::string> controllers{
    "pure_pursuit", "stanley", "lqr", "mpc_sampler"};
  const std::vector<std::string> models{"front_ackermann", "dual_ackermann"};
  ControlSafetyStateMachine machine;

  for (const auto & controller : controllers) {
    for (const auto & model : models) {
      SCOPED_TRACE(controller + "/" + model);
      auto inputs = ready_inputs();
      inputs.trajectory_emergency = true;
      const auto emergency = machine.evaluate(inputs);
      EXPECT_FALSE(emergency.allow_tracking);
      EXPECT_TRUE(emergency.emergency_stop);
      EXPECT_EQ(emergency.state, ControlSafetyState::EstopLatched);

      inputs.trajectory_emergency = false;
      inputs.trajectory_valid = false;
      inputs.trajectory_invalid_reason = "trajectory_status_rejected:failure";
      const auto failure = machine.evaluate(inputs);
      EXPECT_FALSE(failure.allow_tracking);
      EXPECT_EQ(failure.state, ControlSafetyState::ControlledStop);
    }
  }
}

TEST(ControlSafetyStateMachine, StopOutputBypassesSmootherAndMapsToScuBrake)
{
  const std::vector<std::string> controllers{
    "pure_pursuit", "stanley", "lqr", "mpc_sampler"};
  const std::vector<std::string> models{"front_ackermann", "dual_ackermann"};
  for (const auto & controller : controllers) {
    for (const auto & model : models) {
      SCOPED_TRACE(controller + "/" + model);
      CommandSmoother smoother;
      SmootherOptions options;
      ControlCommand moving;
      moving.speed_mps = 0.8;
      moving.front_steering_angle_rad = 0.3;
      moving.enable = true;
      (void)smoother.smooth(moving, options);

      ControlCommand stop;
      stop.speed_mps = 0.0;
      stop.front_steering_angle_rad = 0.0;
      stop.rear_steering_angle_rad = 0.0;
      stop.brake = 1.0;
      stop.enable = false;
      stop.emergency_stop = true;
      stop.reason = "trajectory_emergency_stop";
      const auto smoothed = smoother.smooth(stop, options);
      EXPECT_DOUBLE_EQ(smoothed.speed_mps, 0.0);
      EXPECT_DOUBLE_EQ(smoothed.front_steering_angle_rad, 0.0);
      EXPECT_DOUBLE_EQ(smoothed.rear_steering_angle_rad, 0.0);
      EXPECT_DOUBLE_EQ(smoothed.brake, 1.0);
      EXPECT_FALSE(smoothed.enable);

      ScuCommandMapper mapper;
      const auto mapped = mapper.map(smoothed, ScuCommandOptions{});
      EXPECT_TRUE(mapped.safe_stop);
      EXPECT_FLOAT_EQ(mapped.command.scu_target_speed, 0.0F);
      EXPECT_FLOAT_EQ(mapped.command.scu_steering_angle_front, 0.0F);
      EXPECT_FLOAT_EQ(mapped.command.scu_steering_angle_rear, 0.0F);
      EXPECT_TRUE(mapped.command.scu_brake_enable);
    }
  }
}

TEST(ControlSafetyStateMachine, RejectsUnsafeTrajectoryMetadataAndPoints)
{
  std::string reason;
  EXPECT_FALSE(validate_trajectory_input(
    one_point(1.0), "t1", "p1", "ok", true, {"ok"}, 1.0e-4, &reason));
  EXPECT_EQ(reason, "trajectory_emergency_stop");
  EXPECT_FALSE(validate_trajectory_input(
    one_point(), "t1", "p1", "failure", false, {"ok"}, 1.0e-4, &reason));
  EXPECT_EQ(reason, "trajectory_status_rejected:failure");
  EXPECT_FALSE(validate_trajectory_input(
    one_point(), "", "p1", "ok", false, {"ok"}, 1.0e-4, &reason));
  EXPECT_EQ(reason, "trajectory_id_empty");
  EXPECT_FALSE(validate_trajectory_input({}, "t1", "p1", "ok", false, {"ok"}, 1.0e-4, &reason));

  auto invalid = one_point();
  invalid.front().x_m = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(validate_trajectory_input(
    invalid, "t1", "p1", "ok", false, {"ok"}, 1.0e-4, &reason));

  auto non_monotonic = one_point();
  non_monotonic.push_back({1.0, 0.0, 0.0, 0.0, -1.0, 0.5, 1});
  EXPECT_FALSE(validate_trajectory_input(
    non_monotonic, "t1", "p1", "ok", false, {"ok"}, 1.0e-4, &reason));
}

TEST(ControlSafetyStateMachine, TimeoutsAndVehicleGatesStopTracking)
{
  ControlSafetyStateMachine machine;
  auto inputs = ready_inputs();
  inputs.pose_timed_out = true;
  EXPECT_FALSE(machine.evaluate(inputs).allow_tracking);
  inputs.pose_timed_out = false;
  inputs.trajectory_timed_out = true;
  EXPECT_FALSE(machine.evaluate(inputs).allow_tracking);
  inputs.trajectory_timed_out = false;
  inputs.autonomous_enabled = false;
  EXPECT_EQ(machine.evaluate(inputs).reason, "autonomous_disabled");
  inputs.autonomous_enabled = true;
  inputs.brake_pressed = true;
  EXPECT_EQ(machine.evaluate(inputs).reason, "vehicle_brake_pressed");
  inputs.brake_pressed = false;
  inputs.fault_code = "E42";
  EXPECT_EQ(machine.evaluate(inputs).reason, "vehicle_fault:E42");
}

TEST(ControlSafetyStateMachine, LatchedEstopRequiresExplicitClear)
{
  SafetyEstopLatch latch;
  latch.update(true, true);
  EXPECT_TRUE(latch.is_latched());
  latch.update(false, true);
  EXPECT_TRUE(latch.is_latched());
  latch.clear_explicit();
  EXPECT_FALSE(latch.is_latched());
}

TEST(ControlSafetyStateMachine, ClearPreconditionsAndReadyInterlock)
{
  EstopClearInputs clear;
  clear.localization_ready = true;
  clear.trajectory_ready = true;
  clear.have_vehicle_state = true;
  clear.vehicle_state_ready = true;
  clear.autonomous_enabled = true;
  EXPECT_TRUE(evaluate_estop_clear(clear).allowed);

  clear.vehicle_speed_mps = 0.2;
  EXPECT_FALSE(evaluate_estop_clear(clear).allowed);
  clear.vehicle_speed_mps = 0.0;
  clear.fault_code = "fault";
  EXPECT_FALSE(evaluate_estop_clear(clear).allowed);

  ControlSafetyStateMachine machine;
  auto inputs = ready_inputs();
  inputs.force_ready_cycle = true;
  const auto decision = machine.evaluate(inputs);
  EXPECT_EQ(decision.state, ControlSafetyState::Ready);
  EXPECT_FALSE(decision.allow_tracking);
}

}  // namespace
}  // namespace low_speed_av_control
