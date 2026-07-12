#!/usr/bin/env python3
"""Planning-only ROS2 regression tests using a temporary AD Package fixture."""

from __future__ import annotations

import json
import hashlib
import shutil
import tempfile
import time
import unittest
from pathlib import Path

import launch
import launch_ros.actions
import launch_testing
import launch_testing.actions
import pytest
import rclpy
from ament_index_python.packages import get_package_share_directory
from low_speed_av_interfaces.msg import GlobalRoute, RoadnetStatus, Trajectory
from low_speed_av_interfaces.srv import PlanMission, PlanRoute, ReloadRoadnet
from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy


FIXTURE_ROOT: Path | None = None
INVALID_ROOT: Path | None = None


def _prepare_fixtures(sample: Path) -> tuple[Path, Path]:
    root = Path(tempfile.mkdtemp(prefix="phase15_planning_fixture_"))
    valid = root / "valid"
    invalid = root / "invalid"
    shutil.copytree(sample, valid)
    shutil.copytree(sample, invalid)
    charging_path = valid / "semantics" / "charging_points.json"
    charging = json.loads(charging_path.read_text(encoding="utf-8"))
    charging["charging_points"] = [
        {
            "id": "C_TEST",
            "type": "charging",
            "pose": {"x": 4.0, "y": 1.0, "yaw": 0.4636},
            "linked_node_id": "N0003",
            "linked_edge_id": "E_L002_F",
            "linked_s_m": 2.236,
        }
    ]
    charging_path.write_text(json.dumps(charging, indent=2), encoding="utf-8")
    charging_hash = hashlib.sha256(charging_path.read_bytes()).hexdigest()
    valid_manifest_path = valid / "project_manifest.json"
    valid_manifest = json.loads(valid_manifest_path.read_text(encoding="utf-8"))
    valid_manifest["hashes"]["semantics/charging_points.json"] = charging_hash
    valid_manifest_path.write_text(json.dumps(valid_manifest, indent=2), encoding="utf-8")
    checksums_path = valid / "checksums.sha256"
    checksum_lines = checksums_path.read_text(encoding="utf-8").splitlines()
    checksum_lines = [
        f"{charging_hash}  semantics/charging_points.json"
        if line.endswith("  semantics/charging_points.json")
        else line
        for line in checksum_lines
    ]
    checksums_path.write_text("\n".join(checksum_lines) + "\n", encoding="utf-8")
    manifest_path = invalid / "project_manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["schema"] = "malicious_or_unsupported_schema"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    return valid, invalid


@pytest.mark.launch_test
def generate_test_description():
    global FIXTURE_ROOT, INVALID_ROOT
    bringup_share = Path(get_package_share_directory("low_speed_av_bringup"))
    planning_share = Path(get_package_share_directory("low_speed_av_planning"))
    FIXTURE_ROOT, INVALID_ROOT = _prepare_fixtures(bringup_share / "sample_ad_package")
    planning = launch_ros.actions.Node(
        package="low_speed_av_planning",
        executable="planning_node",
        name="low_speed_av_planning",
        output="screen",
        parameters=[
            str(planning_share / "config" / "planning_params.yaml"),
            {
                "roadnet.package_path": str(bringup_share / "sample_ad_package"),
                "planning.use_current_pose_as_start": False,
            },
        ],
    )
    return launch.LaunchDescription(
        [planning, launch_testing.actions.ReadyToTest()]
    ), {"planning": planning}


class TestPlanningServices(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        rclpy.init()
        cls.node = rclpy.create_node("phase15_planning_services_test")
        cls.routes: list[GlobalRoute] = []
        cls.trajectories: list[Trajectory] = []
        cls.subscriptions = [
            cls.node.create_subscription(GlobalRoute, "/planning/global_route", cls.routes.append, 10),
            cls.node.create_subscription(Trajectory, "/planning/trajectory", cls.trajectories.append, 10),
        ]

    @classmethod
    def tearDownClass(cls):
        cls.node.destroy_node()
        rclpy.shutdown()
        if FIXTURE_ROOT is not None:
            shutil.rmtree(FIXTURE_ROOT.parent, ignore_errors=True)

    @classmethod
    def spin_until(cls, predicate, timeout_s: float, label: str):
        deadline = time.monotonic() + timeout_s
        while time.monotonic() < deadline:
            rclpy.spin_once(cls.node, timeout_sec=0.1)
            if predicate():
                return
        raise AssertionError(
            f"timeout waiting for {label}; routes={len(cls.routes)} "
            f"trajectories={len(cls.trajectories)} services={cls.node.get_service_names_and_types()}"
        )

    @classmethod
    def call(cls, service_type, name: str, request, timeout_s: float = 10.0):
        client = cls.node.create_client(service_type, name)
        if not client.wait_for_service(timeout_sec=timeout_s):
            raise AssertionError(f"service unavailable: {name}")
        future = client.call_async(request)
        rclpy.spin_until_future_complete(cls.node, future, timeout_sec=timeout_s)
        if not future.done() or future.result() is None:
            raise AssertionError(f"service timed out: {name}")
        return future.result()

    def test_01_late_subscriber_receives_ready_transient_status(self):
        received: list[RoadnetStatus] = []
        qos = QoSProfile(depth=1)
        qos.reliability = ReliabilityPolicy.RELIABLE
        qos.durability = DurabilityPolicy.TRANSIENT_LOCAL
        subscription = self.node.create_subscription(
            RoadnetStatus, "/planning/roadnet_status", received.append, qos
        )
        self.spin_until(lambda: any(item.ready for item in received), 10.0, "ready RoadnetStatus")
        ready = next(item for item in received if item.ready)
        self.assertEqual(ready.schema_version, "1.1.0")
        self.assertGreater(ready.waypoints, 0)
        self.node.destroy_subscription(subscription)

    def test_02_plan_route_success_and_republish(self):
        self.routes.clear()
        self.trajectories.clear()
        request = PlanRoute.Request()
        request.start_node_id = "N0001"
        request.goal_node_id = "N0003"
        response = self.call(PlanRoute, "/low_speed_av_planning/plan_route", request)
        self.assertTrue(response.success, response.message)
        self.assertGreater(response.route.length_m, 0.0)
        self.spin_until(
            lambda: any(item.status == "ok" and item.points for item in self.trajectories),
            10.0,
            "successful trajectory",
        )
        first_count = len(self.routes)
        self.spin_until(lambda: len(self.routes) > first_count, 3.0, "bounded route republish")

    def test_03_plan_mission_task_parking_and_charging(self):
        for goal_type, goal_id in (("task", "T001"), ("parking", "P001")):
            request = PlanMission.Request()
            request.start_type = "node"
            request.start_id = "N0001"
            request.goal_type = goal_type
            request.goal_id = goal_id
            response = self.call(PlanMission, "/low_speed_av_planning/plan_mission", request)
            self.assertTrue(response.success, f"{goal_type}: {response.message}")
            self.assertGreaterEqual(response.route.length_m, 0.0)
        assert FIXTURE_ROOT is not None
        reload_request = ReloadRoadnet.Request()
        reload_request.package_path = str(FIXTURE_ROOT)
        reload_response = self.call(
            ReloadRoadnet, "/low_speed_av_planning/reload_roadnet", reload_request
        )
        self.assertTrue(reload_response.success, reload_response.message)
        charging_request = PlanMission.Request()
        charging_request.start_type = "node"
        charging_request.start_id = "N0001"
        charging_request.goal_type = "charging"
        charging_request.goal_id = "C_TEST"
        charging_response = self.call(
            PlanMission, "/low_speed_av_planning/plan_mission", charging_request
        )
        self.assertTrue(charging_response.success, charging_response.message)

    def test_04_invalid_goal_publishes_failure_emergency_trajectory(self):
        self.trajectories.clear()
        request = PlanRoute.Request()
        request.start_node_id = "N0001"
        request.goal_node_id = "NO_SUCH_NODE"
        response = self.call(PlanRoute, "/low_speed_av_planning/plan_route", request)
        self.assertFalse(response.success)
        self.spin_until(
            lambda: any(item.emergency_stop and item.status == "failure" for item in self.trajectories),
            10.0,
            "failure emergency trajectory",
        )

    def test_99_reload_invalid_package_fails_closed(self):
        assert INVALID_ROOT is not None
        request = ReloadRoadnet.Request()
        request.package_path = str(INVALID_ROOT)
        response = self.call(ReloadRoadnet, "/low_speed_av_planning/reload_roadnet", request)
        self.assertFalse(response.success)
        self.assertTrue(response.message)
        route_request = PlanRoute.Request()
        route_request.start_node_id = "N0001"
        route_request.goal_node_id = "N0003"
        route_response = self.call(PlanRoute, "/low_speed_av_planning/plan_route", route_request)
        self.assertFalse(route_response.success)


@launch_testing.post_shutdown_test()
class TestPlanningProcessExit(unittest.TestCase):
    def test_process_exits_with_bounded_wait(self, proc_info, planning):
        proc_info.assertWaitForShutdown(process=planning, timeout=10.0)
