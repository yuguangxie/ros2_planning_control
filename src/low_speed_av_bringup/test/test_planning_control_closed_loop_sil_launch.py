#!/usr/bin/env python3
"""Headless Planning-Control-Plant SIL; no Chassis or network process is launched."""

from __future__ import annotations

import math
import json
import os
import shutil
import tempfile
import time
import unittest
from pathlib import Path

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import launch_testing.asserts
import pytest
import rclpy
from ament_index_python.packages import get_package_share_directory
from chassis_interfaces.msg import ScuControlCommand
from diagnostic_msgs.msg import DiagnosticArray
from geometry_msgs.msg import PoseStamped
from low_speed_av_interfaces.msg import (
    ControlCommand,
    ModuleStatus,
    RoadnetStatus,
    Trajectory,
    VehicleState,
)
from low_speed_av_interfaces.srv import PlanRoute, SetControllerAlgorithm
from nav_msgs.msg import Path as PosePath
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
from std_srvs.srv import Trigger


FIXTURE_PARENT: Path | None = None


def _materialize_sample(sample: Path) -> Path:
    global FIXTURE_PARENT
    FIXTURE_PARENT = Path(tempfile.mkdtemp(prefix="phase18_closed_loop_sil_"))
    fixture = FIXTURE_PARENT / "sample"
    shutil.copytree(sample, fixture)
    return fixture


@pytest.mark.launch_test
def generate_test_description():
    bringup_share = Path(get_package_share_directory("low_speed_av_bringup"))
    simulation_share = Path(get_package_share_directory("low_speed_av_simulation"))
    fixture = _materialize_sample(bringup_share / "sample_ad_package")
    planning = launch_ros.actions.Node(
        package="low_speed_av_planning",
        executable="planning_node",
        name="low_speed_av_planning",
        output="screen",
        parameters=[
            str(bringup_share / "config" / "planning_params.yaml"),
            {
                "roadnet.package_path": str(fixture),
                "planning.local_trajectory_from_current_pose": False,
            },
        ],
    )
    control = launch_ros.actions.Node(
        package="low_speed_av_control",
        executable="control_node",
        name="low_speed_av_control",
        output="screen",
        parameters=[
            str(bringup_share / "config" / "control_params.yaml"),
            str(bringup_share / "config" / "control_sim_params.yaml"),
            {
                "controller.algorithm": "pure_pursuit",
                "vehicle.model": "front_ackermann",
            },
        ],
    )
    simulation = launch_ros.actions.Node(
        package="low_speed_av_simulation",
        executable="sim_localization_pose_publisher_node",
        name="sim_localization_pose_publisher",
        output="screen",
        parameters=[
            str(simulation_share / "config" / "closed_loop_simulation_params.yaml"),
            {"roadnet.package_path": str(fixture)},
        ],
    )
    return launch.LaunchDescription(
        [planning, control, simulation, launch_testing.actions.ReadyToTest()]
    ), {"planning": planning, "control": control, "simulation": simulation}


class TestPlanningControlClosedLoopSil(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase18_planning_control_closed_loop_sil_test")
        cls.roadnet: list[RoadnetStatus] = []
        cls.trajectories: list[Trajectory] = []
        cls.commands: list[ControlCommand] = []
        cls.control_status: list[ModuleStatus] = []
        cls.simulation_status: list[ModuleStatus] = []
        cls.poses: list[PoseStamped] = []
        cls.vehicle_states: list[VehicleState] = []
        cls.pose_paths: list[PosePath] = []
        cls.diagnostics: list[DiagnosticArray] = []
        cls.scu_commands: list[ScuControlCommand] = []
        ready_qos = QoSProfile(depth=1)
        ready_qos.reliability = ReliabilityPolicy.RELIABLE
        ready_qos.durability = DurabilityPolicy.TRANSIENT_LOCAL
        cls.subscriptions = [
            cls.node.create_subscription(
                RoadnetStatus, "/planning/roadnet_status", cls.roadnet.append, ready_qos
            ),
            cls.node.create_subscription(
                Trajectory, "/planning/trajectory", cls.trajectories.append, 20
            ),
            cls.node.create_subscription(
                ControlCommand, "/control/command", cls.commands.append, 50
            ),
            cls.node.create_subscription(
                ModuleStatus, "/control/status", cls.control_status.append, 20
            ),
            cls.node.create_subscription(
                ModuleStatus, "/simulation/status", cls.simulation_status.append, 20
            ),
            cls.node.create_subscription(
                PoseStamped, "/localization/pose", cls.poses.append, 20
            ),
            cls.node.create_subscription(
                VehicleState, "/vehicle/state", cls.vehicle_states.append, 20
            ),
            cls.node.create_subscription(
                PosePath, "/simulation/pose_path", cls.pose_paths.append, 20
            ),
            cls.node.create_subscription(
                DiagnosticArray, "/simulation/diagnostics", cls.diagnostics.append, 20
            ),
            cls.node.create_subscription(
                ScuControlCommand,
                "/yunle_chassis/control/scu_control_command",
                cls.scu_commands.append,
                20,
            ),
        ]

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()
        if FIXTURE_PARENT is not None:
            shutil.rmtree(FIXTURE_PARENT, ignore_errors=True)

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.02)
            if predicate():
                return
        last_pose = None
        if cls.poses:
            last_pose = (
                cls.poses[-1].pose.position.x,
                cls.poses[-1].pose.position.y,
            )
        last_command = cls.commands[-1].reason if cls.commands else None
        last_control = cls.control_status[-1].state if cls.control_status else None
        last_simulation = (
            cls.simulation_status[-1].state if cls.simulation_status else None
        )
        metrics = cls.diagnostic_values()
        raise AssertionError(
            f"timeout waiting for {label}; pose={last_pose} command={last_command} "
            f"control={last_control} simulation={last_simulation} "
            f"target_speed={metrics.get('target_speed_mps')} "
            f"plant_speed={metrics.get('current_speed_mps')} "
            f"lateral={metrics.get('lateral_error_rms_m')} "
            f"command_age={metrics.get('command_age_s')} "
            f"timeouts={metrics.get('timeout_count')}"
        )

    @classmethod
    def call(cls, service_type, name: str, request, timeout_s: float = 10.0):
        client = cls.node.create_client(service_type, name)
        if not client.wait_for_service(timeout_sec=timeout_s):
            raise AssertionError(f"service unavailable: {name}")
        future = client.call_async(request)
        rclpy.spin_until_future_complete(cls.node, future, timeout_sec=timeout_s)
        if not future.done() or future.result() is None:
            raise AssertionError(f"service timeout: {name}")
        return future.result()

    @classmethod
    def diagnostic_values(cls):
        if not cls.diagnostics or not cls.diagnostics[-1].status:
            return {}
        return {
            item.key: item.value
            for item in cls.diagnostics[-1].status[0].values
        }

    def test_01_sample_ready_and_route_success(self):
        self.spin_until(
            lambda: any(status.ready for status in self.roadnet),
            10.0,
            "materialized sample ready",
        )
        request = PlanRoute.Request()
        request.start_node_id = "N0001"
        request.goal_node_id = "N0003"
        response = self.call(
            PlanRoute, "/low_speed_av_planning/plan_route", request
        )
        self.assertTrue(response.success, response.message)
        self.assertGreater(response.route.length_m, 0.0)
        self.spin_until(
            lambda: any(item.status == "ok" and item.points for item in self.trajectories),
            10.0,
            "finite Planning trajectory",
        )
        self.assertTrue(
            all(
                math.isfinite(value)
                for point in self.trajectories[-1].points
                for value in (point.x_m, point.y_m, point.yaw_rad, point.v_mps)
            )
        )

    def test_02_all_controllers_and_vehicle_models_drive_production_plant(self):
        self.spin_until(
            lambda: bool(self.poses) and bool(self.vehicle_states),
            5.0,
            "initial localization and plant VehicleState",
        )
        initial_distance = math.hypot(
            self.poses[-1].pose.position.x, self.poses[-1].pose.position.y
        )
        for model in ("front_ackermann", "dual_ackermann"):
            for algorithm in ("pure_pursuit", "stanley", "lqr", "mpc_sampler"):
                request = SetControllerAlgorithm.Request()
                request.controller_algorithm = algorithm
                request.vehicle_model = model
                response = self.call(
                    SetControllerAlgorithm,
                    "/low_speed_av_control/set_controller_algorithm",
                    request,
                )
                self.assertTrue(response.success, response.message)
                self.spin_until(
                    lambda: any(
                        command.enable
                        and command.controller_algorithm == algorithm
                        and command.vehicle_model == model
                        and all(
                            math.isfinite(value)
                            for value in (
                                command.speed_mps,
                                command.front_steering_angle_rad,
                                command.rear_steering_angle_rad,
                            )
                        )
                        for command in self.commands[-100:]
                    ),
                    4.0,
                    f"ACTIVE finite {algorithm}/{model}",
                )
        self.spin_until(
            lambda: self.poses
            and math.hypot(
                self.poses[-1].pose.position.x, self.poses[-1].pose.position.y
            )
            > initial_distance + 0.1,
            5.0,
            "ControlCommand-driven pose change",
        )
        self.assertTrue(self.vehicle_states)
        self.assertTrue(self.vehicle_states[-1].autonomous_enabled)
        self.assertEqual(self.vehicle_states[-1].gear, 1)
        self.assertTrue(any(len(path.poses) > 2 for path in self.pose_paths))

    def test_03_goal_stop_and_sil_metrics(self):
        self.spin_until(
            lambda: any(status.state == "arrived" for status in self.simulation_status)
            and self.vehicle_states
            and abs(self.vehicle_states[-1].speed_mps) < 0.05,
            20.0,
            "goal tolerance and stopped plant",
        )
        self.spin_until(
            lambda: self.diagnostic_values().get("arrived") == "true"
            and float(self.diagnostic_values().get("stopped_speed_mps", "inf"))
            < 0.05,
            2.0,
            "post-stop SIL diagnostic sample",
        )
        metrics = self.diagnostic_values()
        metrics_path = Path(os.environ.get("ROS_LOG_DIR", "/tmp")) / "sil_metrics.json"
        metrics_path.parent.mkdir(parents=True, exist_ok=True)
        metrics_path.write_text(
            json.dumps(
                {
                    "controller_matrix": [
                        f"{algorithm}/{model}"
                        for model in ("front_ackermann", "dual_ackermann")
                        for algorithm in (
                            "pure_pursuit",
                            "stanley",
                            "lqr",
                            "mpc_sampler",
                        )
                    ],
                    "metrics": metrics,
                    "hardware_boundary": "SIL_ONLY_HIL_NOT_EXECUTED",
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        required = (
            "control_interval_max_s",
            "localization_interval_max_s",
            "lateral_error_rms_m",
            "lateral_error_max_m",
            "goal_distance_m",
            "goal_yaw_error_rad",
            "stopped_speed_mps",
            "stop_response_time_s",
            "non_finite_count",
            "timeout_count",
        )
        self.assertTrue(all(key in metrics for key in required), metrics)
        self.assertLess(float(metrics["control_interval_max_s"]), 0.15)
        self.assertLess(float(metrics["localization_interval_max_s"]), 0.15)
        self.assertLess(float(metrics["lateral_error_rms_m"]), 0.4)
        self.assertLess(float(metrics["lateral_error_max_m"]), 0.8)
        self.assertLess(float(metrics["goal_distance_m"]), 0.3)
        self.assertLess(float(metrics["goal_yaw_error_rad"]), 0.35)
        self.assertLess(float(metrics["stopped_speed_mps"]), 0.05)
        self.assertEqual(int(metrics["non_finite_count"]), 0)

    def test_04_planning_failure_actively_stops_plant(self):
        reset = self.call(Trigger, "/simulation/reset", Trigger.Request())
        self.assertTrue(reset.success, reset.message)
        request = PlanRoute.Request()
        request.start_node_id = "N0001"
        request.goal_node_id = "N0003"
        response = self.call(
            PlanRoute, "/low_speed_av_planning/plan_route", request
        )
        self.assertTrue(response.success, response.message)
        self.spin_until(
            lambda: self.vehicle_states and self.vehicle_states[-1].speed_mps > 0.1,
            5.0,
            "plant moving before Planning failure",
        )
        invalid = PlanRoute.Request()
        invalid.start_node_id = "N0001"
        invalid.goal_node_id = "NO_SUCH_NODE"
        invalid_response = self.call(
            PlanRoute, "/low_speed_av_planning/plan_route", invalid
        )
        self.assertFalse(invalid_response.success)
        self.spin_until(
            lambda: any(
                command.emergency_stop and not command.enable
                for command in self.commands[-100:]
            )
            and self.vehicle_states
            and abs(self.vehicle_states[-1].speed_mps) < 0.05,
            5.0,
            "Planning failure emergency command and stopped plant",
        )

    def test_99_internal_only_and_hardware_isolation(self):
        self.assertEqual(self.scu_commands, [])
        nodes = set(self.node.get_node_names())
        self.assertNotIn("chassis_driver_node", nodes)
        self.assertNotIn("keyboard_scu_control_node", nodes)


@launch_testing.post_shutdown_test()
class TestPlanningControlClosedLoopSilExit(unittest.TestCase):
    def test_processes_exit_cleanly(self, proc_info, planning, control, simulation):
        launch_testing.asserts.assertExitCodes(proc_info, process=planning)
        launch_testing.asserts.assertExitCodes(proc_info, process=control)
        launch_testing.asserts.assertExitCodes(proc_info, process=simulation)
