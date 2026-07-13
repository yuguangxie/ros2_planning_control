#!/usr/bin/env python3
"""Bounded ROS integration for the production closed-loop plant node."""

from __future__ import annotations

import math
import time
import unittest

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import launch_testing.asserts
import pytest
import rclpy
from diagnostic_msgs.msg import DiagnosticArray
from geometry_msgs.msg import PoseStamped
from low_speed_av_interfaces.msg import ControlCommand, ModuleStatus, VehicleState
from nav_msgs.msg import Path
from std_srvs.srv import Trigger


@pytest.mark.launch_test
def generate_test_description():
    simulation = launch_ros.actions.Node(
        package="low_speed_av_simulation",
        executable="sim_localization_pose_publisher_node",
        name="sim_localization_pose_publisher",
        output="screen",
        parameters=[
            {
                "simulation.mode": "control_closed_loop",
                "simulation.closed_loop.plant_step_rate_hz": 100.0,
                "simulation.closed_loop.command_timeout_s": 0.2,
                "simulation.closed_loop.plant.max_acceleration_mps2": 1.0,
                "simulation.closed_loop.plant.max_deceleration_mps2": 1.0,
                "simulation.closed_loop.plant.max_jerk_mps3": 0.0,
                "simulation.localization.publish_rate_hz": 20.0,
                "simulation.localization.initial_pose.source": "explicit",
                "simulation.localization.initial_pose.x": 0.0,
                "simulation.localization.initial_pose.y": 0.0,
                "simulation.localization.initial_pose.yaw": 0.0,
                "reset_clears_path": True,
            }
        ],
    )
    return launch.LaunchDescription(
        [simulation, launch_testing.actions.ReadyToTest()]
    ), {"simulation": simulation}


class TestControlClosedLoopPlant(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase18_control_closed_loop_plant_test")
        cls.poses: list[PoseStamped] = []
        cls.states: list[VehicleState] = []
        cls.statuses: list[ModuleStatus] = []
        cls.diagnostics: list[DiagnosticArray] = []
        cls.paths: list[Path] = []
        cls.node.create_subscription(PoseStamped, "/localization/pose", cls.poses.append, 10)
        cls.node.create_subscription(VehicleState, "/vehicle/state", cls.states.append, 10)
        cls.node.create_subscription(ModuleStatus, "/simulation/status", cls.statuses.append, 10)
        cls.node.create_subscription(
            DiagnosticArray, "/simulation/diagnostics", cls.diagnostics.append, 10
        )
        cls.node.create_subscription(Path, "/simulation/pose_path", cls.paths.append, 10)
        cls.command_pub = cls.node.create_publisher(
            ControlCommand, "/control/command", 10
        )

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.02)
            if predicate():
                return
        last_pose = cls.poses[-1].pose.position.x if cls.poses else None
        last_speed = cls.states[-1].speed_mps if cls.states else None
        last_status = cls.statuses[-1].state if cls.statuses else None
        raise AssertionError(
            f"timeout waiting for {label}; pose_x={last_pose} speed={last_speed} "
            f"simulation_state={last_status} diagnostics={len(cls.diagnostics)}"
        )

    @classmethod
    def publish_drive_for(cls, duration_s: float):
        command = ControlCommand()
        command.speed_mps = 0.5
        command.acceleration_mps2 = 1.0
        command.front_steering_angle_rad = 0.0
        command.rear_steering_angle_rad = 0.0
        command.gear = 1
        command.enable = True
        deadline = time.monotonic() + duration_s
        while time.monotonic() < deadline:
            command.header.stamp = cls.node.get_clock().now().to_msg()
            cls.command_pub.publish(command)
            rclpy.spin_once(cls.node, timeout_sec=0.04)

    @classmethod
    def call_reset(cls):
        client = cls.node.create_client(Trigger, "/simulation/reset")
        if not client.wait_for_service(timeout_sec=5.0):
            raise AssertionError("simulation reset service unavailable")
        future = client.call_async(Trigger.Request())
        rclpy.spin_until_future_complete(cls.node, future, timeout_sec=5.0)
        if not future.done() or future.result() is None:
            raise AssertionError("simulation reset service timed out")
        return future.result()

    def test_control_command_drives_plant_and_timeout_stops(self):
        self.spin_until(
            lambda: self.poses and self.states and self.diagnostics,
            5.0,
            "initial closed-loop publications",
        )
        initial_x = self.poses[-1].pose.position.x
        self.publish_drive_for(1.0)
        self.spin_until(
            lambda: self.poses[-1].pose.position.x > initial_x + 0.1
            and self.states[-1].speed_mps > 0.1,
            3.0,
            "command-driven pose and vehicle state",
        )
        self.assertTrue(
            all(
                math.isfinite(value)
                for value in (
                    self.poses[-1].pose.position.x,
                    self.poses[-1].pose.position.y,
                    self.states[-1].speed_mps,
                    self.states[-1].acceleration_mps2,
                )
            )
        )
        self.assertTrue(self.states[-1].autonomous_enabled)
        self.assertFalse(self.states[-1].brake_pressed)

        self.spin_until(
            lambda: self.states and abs(self.states[-1].speed_mps) < 0.05,
            3.0,
            "command timeout stop",
        )
        self.spin_until(
            lambda: any(status.state == "command_timeout" for status in self.statuses),
            2.0,
            "command timeout status",
        )
        diagnostic_values = {
            item.key: item.value
            for array in self.diagnostics
            for status in array.status
            for item in status.values
        }
        self.assertEqual(diagnostic_values.get("mode"), "control_closed_loop")
        self.assertGreaterEqual(int(diagnostic_values.get("timeout_count", "0")), 1)
        self.assertEqual(diagnostic_values.get("non_finite_count"), "0")

        response = self.call_reset()
        self.assertTrue(response.success, response.message)
        self.paths.clear()
        self.spin_until(
            lambda: any(
                len(path.poses) == 1
                and abs(path.poses[0].pose.position.x) < 1e-9
                and abs(path.poses[0].pose.position.y) < 1e-9
                for path in self.paths
            ),
            3.0,
            "reset plant and history",
        )

        publisher_nodes = {
            info.node_name
            for info in self.node.get_publishers_info_by_topic(
                "/yunle_chassis/control/scu_control_command"
            )
        }
        self.assertEqual(publisher_nodes, set())
        node_names = set(self.node.get_node_names())
        self.assertNotIn("chassis_driver_node", node_names)
        self.assertNotIn("keyboard_scu_control_node", node_names)


@launch_testing.post_shutdown_test()
class TestControlClosedLoopPlantExit(unittest.TestCase):
    def test_process_exits_cleanly(self, proc_info, simulation):
        launch_testing.asserts.assertExitCodes(proc_info, process=simulation)
