#!/usr/bin/env python3
"""Bounded Control integration tests. The Chassis Driver is never launched."""

from __future__ import annotations

import time
import unittest

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import pytest
import rclpy
from ament_index_python.packages import get_package_share_directory
from chassis_interfaces.msg import ScuControlCommand
from geometry_msgs.msg import PoseStamped
from low_speed_av_interfaces.msg import (
    ControlCommand,
    ModuleStatus,
    Trajectory,
    TrajectoryPoint,
    VehicleState,
)
from low_speed_av_interfaces.srv import SetControllerAlgorithm
from std_srvs.srv import Trigger


@pytest.mark.launch_test
def generate_test_description():
    control_share = get_package_share_directory("low_speed_av_control")
    control = launch_ros.actions.Node(
        package="low_speed_av_control",
        executable="control_node",
        name="low_speed_av_control",
        output="screen",
        parameters=[
            f"{control_share}/config/control_params.yaml",
            {
                "vehicle_state.required": True,
                "controller.localization_timeout_s": 0.25,
                "controller.trajectory_timeout_s": 0.25,
                "vehicle_state.timeout_s": 0.25,
                "control.status_publish_rate_hz": 10.0,
            },
        ],
    )
    return launch.LaunchDescription(
        [control, launch_testing.actions.ReadyToTest()]
    ), {"control": control}


class TestControlRuntime(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase16_control_runtime_test")
        cls.pose_pub = cls.node.create_publisher(PoseStamped, "/localization/pose", 10)
        cls.trajectory_pub = cls.node.create_publisher(Trajectory, "/planning/trajectory", 10)
        cls.vehicle_pub = cls.node.create_publisher(VehicleState, "/vehicle/state", 10)
        cls.safety_pub = cls.node.create_publisher(ModuleStatus, "/safety/status", 10)
        cls.commands: list[ControlCommand] = []
        cls.command_times: list[float] = []
        cls.scu_commands: list[ScuControlCommand] = []
        cls.scu_times: list[float] = []
        cls.statuses: list[ModuleStatus] = []
        cls.subscriptions = [
            cls.node.create_subscription(
                ControlCommand, "/control/command", cls._on_command, 50
            ),
            cls.node.create_subscription(
                ScuControlCommand,
                "/yunle_chassis/control/scu_control_command",
                cls._on_scu,
                50,
            ),
            cls.node.create_subscription(
                ModuleStatus, "/control/status", cls.statuses.append, 20
            ),
        ]

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    @classmethod
    def _on_command(cls, message: ControlCommand):
        cls.commands.append(message)
        cls.command_times.append(time.monotonic())

    @classmethod
    def _on_scu(cls, message: ScuControlCommand):
        cls.scu_commands.append(message)
        cls.scu_times.append(time.monotonic())

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.01)
            if predicate():
                return
        last_command = cls.commands[-1].reason if cls.commands else "none"
        last_status = cls.statuses[-1].message if cls.statuses else "none"
        raise AssertionError(
            f"timeout waiting for {label}; commands={len(cls.commands)} "
            f"scu={len(cls.scu_commands)} last_command={last_command} "
            f"last_status={last_status}"
        )

    @classmethod
    def valid_messages(cls, trajectory_id: str = "phase16-trajectory"):
        pose = PoseStamped()
        pose.header.frame_id = "map"
        pose.pose.orientation.w = 1.0
        vehicle = VehicleState()
        vehicle.speed_mps = 0.0
        vehicle.gear = 1
        vehicle.autonomous_enabled = True
        trajectory = Trajectory()
        trajectory.trajectory_id = trajectory_id
        trajectory.source_package_id = "phase16-fixture"
        trajectory.status = "ok"
        for index in range(4):
            point = TrajectoryPoint()
            point.index = index
            point.x_m = float(index)
            point.s_m = float(index)
            point.v_mps = 0.3
            point.gear = 1
            trajectory.points.append(point)
        return pose, vehicle, trajectory

    @classmethod
    def publish_valid_inputs(cls, trajectory_id: str = "phase16-trajectory", count: int = 3):
        pose, vehicle, trajectory = cls.valid_messages(trajectory_id)
        for _ in range(count):
            cls.pose_pub.publish(pose)
            cls.vehicle_pub.publish(vehicle)
            cls.trajectory_pub.publish(trajectory)
            rclpy.spin_once(cls.node, timeout_sec=0.02)

    @classmethod
    def call(cls, service_type, name: str, request, timeout_s: float = 5.0):
        client = cls.node.create_client(service_type, name)
        if not client.wait_for_service(timeout_sec=timeout_s):
            raise AssertionError(f"service unavailable: {name}")
        future = client.call_async(request)
        rclpy.spin_until_future_complete(cls.node, future, timeout_sec=timeout_s)
        if not future.done() or future.result() is None:
            raise AssertionError(f"service timeout: {name}")
        return future.result()

    def test_01_default_both_outputs_are_periodic_with_watchdog_margin(self):
        self.publish_valid_inputs()
        start_internal = len(self.commands)
        start_scu = len(self.scu_commands)
        self.spin_until(
            lambda: len(self.commands) >= start_internal + 20
            and len(self.scu_commands) >= start_scu + 20,
            5.0,
            "20 periodic internal and SCU commands",
        )
        internal_intervals = [
            later - earlier
            for earlier, later in zip(self.command_times[-20:-1], self.command_times[-19:])
        ]
        scu_intervals = [
            later - earlier
            for earlier, later in zip(self.scu_times[-20:-1], self.scu_times[-19:])
        ]
        self.assertTrue(internal_intervals)
        self.assertLess(max(internal_intervals), 0.15)
        self.assertLess(max(scu_intervals), 0.15)
        self.spin_until(
            lambda: any("hardware_watchdog=DECLARED_NOT_HIL_VERIFIED" in item.message for item in self.statuses),
            3.0,
            "cadence and hardware contract diagnostics",
        )

    def test_02_all_controller_switches_reset_and_preserve_valid_output(self):
        for algorithm in ("pure_pursuit", "stanley", "lqr", "mpc_sampler"):
            request = SetControllerAlgorithm.Request()
            request.controller_algorithm = algorithm
            request.vehicle_model = "front_ackermann"
            response = self.call(
                SetControllerAlgorithm,
                "/low_speed_av_control/set_controller_algorithm",
                request,
            )
            self.assertTrue(response.success, response.message)
            self.commands.clear()
            self.publish_valid_inputs(trajectory_id=f"trajectory-{algorithm}")
            self.spin_until(
                lambda: any(item.enable and item.controller_algorithm == algorithm for item in self.commands),
                3.0,
                f"active output for {algorithm}",
            )

    def test_03_vehicle_state_gates_fail_closed(self):
        pose, vehicle, trajectory = self.valid_messages("vehicle-gates")
        for reason, mutate in (
            ("autonomous_disabled", lambda state: setattr(state, "autonomous_enabled", False)),
            ("vehicle_brake_pressed", lambda state: setattr(state, "brake_pressed", True)),
            ("vehicle_fault:E42", lambda state: setattr(state, "fault_code", "E42")),
        ):
            vehicle = self.valid_messages("vehicle-gates")[1]
            mutate(vehicle)
            self.commands.clear()
            self.pose_pub.publish(pose)
            self.trajectory_pub.publish(trajectory)
            self.vehicle_pub.publish(vehicle)
            self.spin_until(
                lambda: any(not item.enable and reason in item.reason for item in self.commands),
                2.0,
                reason,
            )

    def test_04_localization_trajectory_and_vehicle_timeouts_stop(self):
        pose, vehicle, trajectory = self.valid_messages("timeout-cases")
        self.publish_valid_inputs("timeout-cases")
        self.commands.clear()
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            self.vehicle_pub.publish(vehicle)
            self.trajectory_pub.publish(trajectory)
            rclpy.spin_once(self.node, timeout_sec=0.02)
        self.assertTrue(any(item.reason == "localization_timeout" for item in self.commands))

        self.publish_valid_inputs("timeout-cases")
        self.commands.clear()
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            self.pose_pub.publish(pose)
            self.vehicle_pub.publish(vehicle)
            rclpy.spin_once(self.node, timeout_sec=0.02)
        self.assertTrue(any(item.reason == "trajectory_timeout" for item in self.commands))

        self.publish_valid_inputs("timeout-cases")
        self.commands.clear()
        deadline = time.monotonic() + 0.5
        while time.monotonic() < deadline:
            self.pose_pub.publish(pose)
            self.trajectory_pub.publish(trajectory)
            rclpy.spin_once(self.node, timeout_sec=0.02)
        self.assertTrue(any(item.reason == "vehicle_state_timeout" for item in self.commands))

    def test_05_estop_latch_requires_clear_and_clear_returns_ready_first(self):
        self.publish_valid_inputs("estop-clear")
        safety = ModuleStatus()
        safety.module_name = "safety"
        safety.state = "estop"
        safety.level = 2
        self.safety_pub.publish(safety)
        self.spin_until(
            lambda: any(item.emergency_stop and not item.enable for item in self.commands),
            2.0,
            "latched estop command",
        )
        rejected = self.call(Trigger, "/low_speed_av_control/clear_estop", Trigger.Request())
        self.assertFalse(rejected.success)

        safety.state = "ok"
        safety.level = 0
        self.safety_pub.publish(safety)
        self.publish_valid_inputs("estop-clear")
        response = self.call(Trigger, "/low_speed_av_control/clear_estop", Trigger.Request())
        self.assertTrue(response.success, response.message)
        self.assertTrue(any(item.state == "READY" for item in self.statuses))
        self.commands.clear()
        self.publish_valid_inputs("estop-clear")
        self.spin_until(
            lambda: any(item.enable for item in self.commands),
            2.0,
            "tracking resumes only after READY interlock",
        )

    def test_06_late_subscriber_receives_future_periodic_command(self):
        received: list[ControlCommand] = []
        subscription = self.node.create_subscription(
            ControlCommand, "/control/command", received.append, 10
        )
        self.publish_valid_inputs("late-subscriber")
        self.spin_until(lambda: bool(received), 2.0, "late subscriber command")
        self.node.destroy_subscription(subscription)


@launch_testing.post_shutdown_test()
class TestControlProcessExit(unittest.TestCase):
    def test_control_exits_with_bounded_wait(self, proc_info, control):
        proc_info.assertWaitForShutdown(process=control, timeout=10.0)
