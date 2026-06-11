#!/usr/bin/env python3
"""Offline checks for semantic-goal planning follow-up fixes.

This script mirrors the non-ROS planning contracts introduced after Ubuntu
runtime revalidation. It intentionally does not modify the generated AD Package.
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

try:
    import yaml
except ImportError as exc:  # pragma: no cover - environment diagnostic
    raise SystemExit(f"PyYAML is required for this smoke script: {exc}") from exc


def load_package(root: Path) -> dict:
    with (root / "roadnet" / "topology.json").open("r", encoding="utf-8") as f:
        topology = json.load(f)
    with (root / "semantics" / "task_points.json").open("r", encoding="utf-8") as f:
        task_points = json.load(f)["task_points"]
    with (root / "trajectory" / "waypoints.yaml").open("r", encoding="utf-8") as f:
        waypoints = yaml.safe_load(f)["waypoints"]
    with (root / "trajectory" / "waypoint_index.json").open("r", encoding="utf-8") as f:
        waypoint_index = json.load(f)["edges"]
    return {
        "edges": {edge["id"]: edge for edge in topology["edges"]},
        "task_points": {point["id"]: point for point in task_points},
        "waypoints": waypoints,
        "waypoint_index": waypoint_index,
    }


def clean_optional_string(value) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    return "" if text == "" or text.lower() in {"null", "none"} else text


def edge_range(index_entry: dict) -> tuple[int, int]:
    start = int(index_entry["start_index"])
    if "end_index_exclusive" in index_entry:
        return start, int(index_entry["end_index_exclusive"])
    return start, int(index_entry["end_index"]) + 1


def project_point_to_edge(point: dict, package: dict) -> dict:
    edge_id = clean_optional_string(point.get("linked_edge_id"))
    if not edge_id:
        edge_id = clean_optional_string(point.get("entry_edge_id"))
    if not edge_id and isinstance(point.get("approach"), dict):
        edge_id = clean_optional_string(point["approach"].get("edge_id"))
    if edge_id not in package["edges"]:
        raise ValueError("semantic point has no valid linked_node_id or linked_edge_id")
    start, end = edge_range(package["waypoint_index"][edge_id])
    px = point["pose"]["x"]
    py = point["pose"]["y"]
    best_index = min(
        range(start, end),
        key=lambda i: math.hypot(package["waypoints"][i]["x"] - px, package["waypoints"][i]["y"] - py),
    )
    progress = (best_index - start) / max(1, end - start - 1)
    edge = package["edges"][edge_id]
    return {
        "point_id": point["id"],
        "edge_id": edge_id,
        "from_node": edge["from"],
        "to_node": edge["to"],
        "fallback_goal_node": edge["to"],
        "fallback_start_node": edge["from"],
        "waypoint_index": best_index,
        "edge_progress": progress,
        "x": px,
        "y": py,
        "yaw": point["pose"]["yaw"],
    }


def crop_edge_segment(anchor: dict, package: dict, reverse: bool) -> list[dict]:
    start, end = edge_range(package["waypoint_index"][anchor["edge_id"]])
    goal_index = anchor["waypoint_index"]
    if reverse:
        indexes = list(range(end - 1, goal_index - 1, -1))
        gear = 2
    else:
        indexes = list(range(start, goal_index + 1))
        gear = 1
    segment = [dict(package["waypoints"][i], gear=gear) for i in indexes]
    assert segment, "cropped segment must not be empty"
    segment[-1]["x"] = anchor["x"]
    segment[-1]["y"] = anchor["y"]
    segment[-1]["yaw"] = anchor["yaw"]
    segment[-1]["v_mps"] = 0.0
    return segment


def distance(a: dict, b: dict) -> float:
    return math.hypot(a["x"] - b["x"], a["y"] - b["y"])


def clamp(value: float, limit: float) -> float:
    return max(-limit, min(limit, value))


def main() -> int:
    if len(sys.argv) > 1:
        root = Path(sys.argv[1])
    else:
        root = Path("roadnet_ad_package_20260610T012525Z_2")
        if not root.exists():
            root = Path("roadnet_ad_package_20260610T012525Z_1")
    package = load_package(root)

    rp001 = project_point_to_edge(package["task_points"]["RP-001"], package)
    rp003 = project_point_to_edge(package["task_points"]["RP-003"], package)
    assert rp001["fallback_goal_node"], "RP-001 goal fallback node is empty"
    assert rp003["fallback_goal_node"] == "N0001", "RP-003 should reproduce the N0001 goal fallback"

    current_pose = {"x": 0.554, "y": 1.473, "yaw": -0.9178, "node": "N0001"}
    assert current_pose["node"] == rp003["fallback_goal_node"], "fixture should reproduce route_N0001_N0001"
    assert distance(current_pose, rp003) > 0.5, "current pose must not be considered arrived at RP-003"
    reverse_segment = crop_edge_segment(rp003, package, reverse=True)
    assert len(reverse_segment) > 1, "current pose -> RP-003 should produce a non-empty reverse local segment"
    assert reverse_segment[-1]["gear"] == 2, "reverse local segment should carry reverse gear"
    assert distance(reverse_segment[-1], rp003) < 1.0e-6, "semantic trajectory endpoint should be RP-003 pose"

    arrived_pose = {"x": rp003["x"], "y": rp003["y"], "yaw": rp003["yaw"]}
    assert distance(arrived_pose, rp003) < 0.5, "arrived fixture should pass arrival radius"

    invalid_task = {"id": "BAD_TASK", "pose": {"x": 0.0, "y": 0.0, "yaw": 0.0}, "linked_edge_id": "BAD_EDGE"}
    try:
      project_point_to_edge(invalid_task, package)
    except ValueError as exc:
      assert "no valid linked_node_id or linked_edge_id" in str(exc)
    else:
      raise AssertionError("invalid task point should fail clearly")

    valid_parking_fixture = {
        "id": "PK-FIXTURE-001",
        "pose": {"x": 0.676, "y": 1.314, "yaw": -0.9366},
        "linked_node_id": None,
        "linked_edge_id": "E_C-001_F",
    }
    parking_anchor = project_point_to_edge(valid_parking_fixture, package)
    parking_segment = crop_edge_segment(parking_anchor, package, reverse=False)
    assert parking_segment, "valid parking fixture should generate a local segment"

    invalid_parking_fixture = {
        "id": "PK-BAD",
        "pose": {"x": 0.0, "y": 0.0, "yaw": 0.0},
        "linked_node_id": "null",
        "linked_edge_id": "BAD_EDGE",
    }
    try:
      project_point_to_edge(invalid_parking_fixture, package)
    except ValueError:
      pass
    else:
      raise AssertionError("invalid parking fixture should fail clearly")

    raw_steering_deg = -29.79
    assert abs(clamp(raw_steering_deg, 27.0)) <= 27.0, "SCU steering clamp must enforce 27 deg"

    planning_yaml = Path("src/low_speed_av_bringup/config/planning_params.yaml").read_text(encoding="utf-8")
    assert "trajectory_republish_rate_hz: 10.0" in planning_yaml
    assert "semantic_goal_allow_reverse_local_segment: false" in planning_yaml
    assert "allow_reverse_planning: false" in planning_yaml
    assert "allow_reverse_local_segment: false" in planning_yaml
    assert "include_current_edge_prefix: true" in planning_yaml
    control_yaml = Path("src/low_speed_av_bringup/config/control_params.yaml").read_text(encoding="utf-8")
    assert "max_steering_angle_deg: 27.0" in control_yaml
    assert "status_publish_rate_hz: 5.0" in control_yaml

    print("offline_semantic_goal_followup_smoke: PASS")
    print("  RP-001 goal anchor:", rp001)
    print("  RP-003 reverse segment points:", len(reverse_segment))
    print("  parking fixture segment points:", len(parking_segment))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
