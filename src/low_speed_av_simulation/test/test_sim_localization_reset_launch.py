#!/usr/bin/env python3
"""Bounded reset regression for the simulated localization publisher."""

from __future__ import annotations

import time
import unittest

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import launch_testing.asserts
import pytest
import rclpy
from low_speed_av_interfaces.msg import Trajectory, TrajectoryPoint
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
                "simulation.localization.mode": "path_follow",
                "simulation.localization.frame_id": "phase17_map",
                "simulation.localization.publish_rate_hz": 20.0,
                "simulation.localization.initial_pose.source": "explicit",
                "simulation.localization.initial_pose.x": 0.0,
                "simulation.localization.initial_pose.y": 0.0,
                "simulation.localization.initial_pose.yaw": 0.0,
                "simulation.localization.follow.follow_source": "trajectory",
                "simulation.localization.follow.default_speed_mps": 1.0,
                "simulation.localization.follow.max_speed_mps": 1.0,
                "simulation.localization.follow.acceleration_limit_mps2": 10.0,
                "reset_clears_path": True,
            }
        ],
    )
    return launch.LaunchDescription(
        [simulation, launch_testing.actions.ReadyToTest()]
    ), {"simulation": simulation}


class TestSimLocalizationReset(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase17_sim_localization_reset_test")
        cls.paths: list[Path] = []
        cls.node.create_subscription(
            Path, "/simulation/pose_path", cls.paths.append, 10
        )
        cls.trajectory_pub = cls.node.create_publisher(
            Trajectory, "/planning/trajectory", 10
        )

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.05)
            if predicate():
                return
        last_size = len(cls.paths[-1].poses) if cls.paths else 0
        raise AssertionError(
            f"timeout waiting for {label}; paths={len(cls.paths)} last_size={last_size}"
        )

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

    @staticmethod
    def moving_trajectory() -> Trajectory:
        trajectory = Trajectory()
        trajectory.trajectory_id = "phase17-reset-path"
        trajectory.source_package_id = "phase17-fixture"
        trajectory.status = "ok"
        for index, x in enumerate((0.0, 2.0)):
            point = TrajectoryPoint()
            point.index = index
            point.x_m = x
            point.y_m = 0.0
            point.yaw_rad = 0.0
            point.s_m = x
            point.v_mps = 1.0
            point.gear = 1
            trajectory.points.append(point)
        return trajectory

    def test_reset_restarts_pose_history_from_initial_pose(self):
        self.spin_until(
            lambda: any(len(path.poses) == 1 for path in self.paths),
            5.0,
            "initial one-pose history",
        )
        trajectory = self.moving_trajectory()
        for _ in range(5):
            self.trajectory_pub.publish(trajectory)
            rclpy.spin_once(self.node, timeout_sec=0.05)
        self.spin_until(
            lambda: any(
                len(path.poses) >= 3 and path.poses[-1].pose.position.x > 0.05
                for path in self.paths
            ),
            5.0,
            "moving pose history",
        )
        old_stamp_ns = max(
            path.header.stamp.sec * 1_000_000_000 + path.header.stamp.nanosec
            for path in self.paths
        )

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
            5.0,
            "reset one-pose history",
        )
        reset_path = next(path for path in self.paths if len(path.poses) == 1)
        reset_stamp_ns = (
            reset_path.header.stamp.sec * 1_000_000_000
            + reset_path.header.stamp.nanosec
        )
        pose_stamp_ns = (
            reset_path.poses[0].header.stamp.sec * 1_000_000_000
            + reset_path.poses[0].header.stamp.nanosec
        )
        self.assertEqual(reset_path.header.frame_id, "phase17_map")
        self.assertEqual(reset_path.poses[0].header.frame_id, "phase17_map")
        self.assertGreater(reset_stamp_ns, old_stamp_ns)
        self.assertGreater(pose_stamp_ns, 0)


@launch_testing.post_shutdown_test()
class TestSimLocalizationProcessExit(unittest.TestCase):
    def test_process_exits_cleanly(self, proc_info, simulation):
        launch_testing.asserts.assertExitCodes(proc_info, process=simulation)
