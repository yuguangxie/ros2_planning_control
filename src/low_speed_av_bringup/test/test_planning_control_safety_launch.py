#!/usr/bin/env python3
"""ROS2 production integration test; never starts the real chassis driver."""

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
from geometry_msgs.msg import PoseStamped

from chassis_interfaces.msg import ScuControlCommand
from low_speed_av_interfaces.msg import ControlCommand, Trajectory, VehicleState
from low_speed_av_interfaces.srv import PlanRoute


@pytest.mark.launch_test
def generate_test_description():
    bringup_share = get_package_share_directory("low_speed_av_bringup")
    planning_share = get_package_share_directory("low_speed_av_planning")
    control_share = get_package_share_directory("low_speed_av_control")
    planning = launch_ros.actions.Node(
        package="low_speed_av_planning",
        executable="planning_node",
        name="low_speed_av_planning",
        output="screen",
        parameters=[
            f"{planning_share}/config/planning_params.yaml",
            {"roadnet.package_path": f"{bringup_share}/sample_ad_package"},
        ],
    )
    control = launch_ros.actions.Node(
        package="low_speed_av_control",
        executable="control_node",
        name="low_speed_av_control",
        output="screen",
        parameters=[f"{control_share}/config/control_params.yaml"],
    )
    return (
        launch.LaunchDescription(
            [planning, control, launch_testing.actions.ReadyToTest()]
        ),
        {"planning": planning, "control": control},
    )


class TestPlanningControlSafety(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase14_planning_control_test")
        cls.pose_pub = cls.node.create_publisher(PoseStamped, "/localization/pose", 10)
        cls.vehicle_pub = cls.node.create_publisher(VehicleState, "/vehicle/state", 10)
        cls.trajectories: list[Trajectory] = []
        cls.commands: list[ControlCommand] = []
        cls.scu_commands: list[ScuControlCommand] = []
        cls.node.create_subscription(
            Trajectory, "/planning/trajectory", cls.trajectories.append, 10
        )
        cls.node.create_subscription(
            ControlCommand, "/control/command", cls.commands.append, 10
        )
        cls.node.create_subscription(
            ScuControlCommand,
            "/yunle_chassis/control/scu_control_command",
            cls.scu_commands.append,
            10,
        )

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.1)
            if predicate():
                return
        raise AssertionError(
            f"timeout waiting for {label}; trajectories={len(cls.trajectories)} "
            f"commands={len(cls.commands)} scu={len(cls.scu_commands)}"
        )

    def test_invalid_goal_propagates_emergency_brake_to_both_outputs(self):
        pose = PoseStamped()
        pose.header.frame_id = "map"
        pose.pose.orientation.w = 1.0
        vehicle = VehicleState()
        vehicle.autonomous_enabled = True
        vehicle.gear = 1
        for _ in range(3):
            self.pose_pub.publish(pose)
            self.vehicle_pub.publish(vehicle)
            rclpy.spin_once(self.node, timeout_sec=0.05)

        client = self.node.create_client(PlanRoute, "/low_speed_av_planning/plan_route")
        self.assertTrue(client.wait_for_service(timeout_sec=10.0), "PlanRoute service unavailable")
        request = PlanRoute.Request()
        request.start_node_id = "N0001"
        request.goal_node_id = "NO_SUCH_NODE"
        future = client.call_async(request)
        rclpy.spin_until_future_complete(self.node, future, timeout_sec=10.0)
        self.assertTrue(future.done(), "PlanRoute response timed out")
        self.assertFalse(future.result().success)

        self.spin_until(
            lambda: any(item.emergency_stop for item in self.trajectories),
            10.0,
            "planning emergency trajectory",
        )
        self.spin_until(
            lambda: any(not item.enable and item.brake > 0.0 for item in self.commands),
            10.0,
            "internal brake stop",
        )
        self.spin_until(
            lambda: any(item.scu_brake_enable for item in self.scu_commands),
            10.0,
            "SCU brake stop",
        )
        internal = next(item for item in reversed(self.commands) if not item.enable)
        scu = next(item for item in reversed(self.scu_commands) if item.scu_brake_enable)
        self.assertEqual(internal.speed_mps, 0.0)
        self.assertTrue(internal.reason)
        self.assertEqual(scu.scu_target_speed, 0.0)

    @unittest.skip("SKIPPED_KNOWN_PRODUCTION_GAP: CDX-P0-002")
    def test_chassis_publisher_loss_triggers_watchdog_stop(self):
        """Specification retained until the production chassis watchdog exists."""


@launch_testing.post_shutdown_test()
class TestProcessExit(unittest.TestCase):
    def test_processes_exit_cleanly(self, proc_info, planning, control):
        proc_info.assertWaitForShutdown(process=planning, timeout=10.0)
        proc_info.assertWaitForShutdown(process=control, timeout=10.0)
