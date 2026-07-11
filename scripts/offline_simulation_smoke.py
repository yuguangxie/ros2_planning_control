#!/usr/bin/env python3
"""Offline smoke checks for simulation visualization and current-pose planning.

This script intentionally avoids ROS2 imports. It validates the data and
matching logic that the ROS2 simulation/planning nodes use at runtime.
"""
from __future__ import annotations

import argparse
import heapq
import json
import math
from pathlib import Path

try:
    import yaml
except Exception:  # pragma: no cover
    yaml = None


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_yaml(path: Path):
    if yaml is None:
        raise RuntimeError("PyYAML required; run with: uv run --with pyyaml python scripts/offline_simulation_smoke.py")
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def yaw_to_quaternion(yaw: float):
    return {"x": 0.0, "y": 0.0, "z": math.sin(yaw * 0.5), "w": math.cos(yaw * 0.5)}


def quat_norm(q) -> float:
    return math.sqrt(q["x"] ** 2 + q["y"] ** 2 + q["z"] ** 2 + q["w"] ** 2)


def normalize_angle(angle: float) -> float:
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def dijkstra(topology, start: str, goal: str):
    adj = {}
    for edge in topology["edges"]:
        adj.setdefault(edge["from"], []).append(edge)
    pq = [(0.0, start, [])]
    seen = {}
    while pq:
        cost, node, path = heapq.heappop(pq)
        if node in seen and seen[node] <= cost:
            continue
        seen[node] = cost
        if node == goal:
            return path
        for edge in adj.get(node, []):
            if not edge.get("availability", {}).get("enabled", True):
                continue
            heapq.heappush(pq, (cost + float(edge.get("cost", 1.0)), edge["to"], path + [edge["id"]]))
    return []


def stitch(edge_ids, waypoint_index, waypoints):
    out = []
    for edge_id in edge_ids:
        rng = waypoint_index["edges"][edge_id]
        start = int(rng["start_index"])
        end = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
        part = waypoints[start:end]
        if out and part and out[-1]["waypoint_id"] == part[0]["waypoint_id"]:
            part = part[1:]
        out.extend(part)
    route_s = 0.0
    prev = None
    for wp in out:
        if prev is not None:
            route_s += math.hypot(float(wp["x"]) - float(prev["x"]), float(wp["y"]) - float(prev["y"]))
        wp["route_s_m"] = route_s
        prev = wp
    return out


def match_pose_to_start_node(topology, waypoint_index, waypoints, pose, max_dist=3.0, max_heading=1.57):
    best = None
    best_dist = float("inf")
    best_heading = float("inf")
    for wp in waypoints:
        heading = abs(normalize_angle(float(pose["yaw"]) - float(wp["yaw"])))
        if heading > max_heading:
            continue
        dist = math.hypot(float(pose["x"]) - float(wp["x"]), float(pose["y"]) - float(wp["y"]))
        if dist < best_dist:
            best = wp
            best_dist = dist
            best_heading = heading
    if best is None:
        raise AssertionError("current pose did not match any waypoint heading")
    if best_dist > max_dist:
        raise AssertionError(f"nearest waypoint too far: {best_dist:.3f} m")
    edge = next(edge for edge in topology["edges"] if edge["id"] == best["edge_id"])
    rng = waypoint_index["edges"][best["edge_id"]]
    start = int(rng["start_index"])
    end = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
    count = max(1, end - start)
    progress = (int(best["global_index"]) - start) / max(1, count - 1)
    start_node = edge["to"] if progress >= 0.5 else edge["from"]
    return {
        "start_node": start_node,
        "waypoint_id": best["waypoint_id"],
        "edge_id": best["edge_id"],
        "distance_m": best_dist,
        "heading_error_rad": best_heading,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "package", nargs="?", default="roadnet_ad_package_20260610T012525Z_2",
        help="Roadnet package (default: deterministic _2 fixture)",
    )
    args = parser.parse_args()
    root = Path(args.package)
    if not root.exists():
        raise SystemExit(f"missing AD package: {root}")

    manifest = load_json(root / "project_manifest.json")
    topology = load_json(root / manifest["files"].get("topology", "roadnet/topology.json"))
    waypoints = load_yaml(root / manifest["files"].get("waypoints_yaml", "trajectory/waypoints.yaml"))["waypoints"]
    waypoint_index = load_json(root / manifest["files"].get("waypoint_index", "trajectory/waypoint_index.json"))
    areas = load_json(root / manifest["files"].get("areas", "semantics/areas.json"))["areas"]
    task_points = load_json(root / manifest["files"].get("task_points", "semantics/task_points.json"))["task_points"]

    assert topology["nodes"], "roadnet marker generation would be empty: no nodes"
    assert topology["edges"], "roadnet marker generation would be empty: no edges"
    assert waypoints, "roadnet marker generation would be empty: no waypoints"
    # Semantic areas are optional in editor exports. Visualization must still
    # publish topology, waypoints, and semantic point markers when areas are
    # empty.
    assert task_points, "semantic point marker generation would be empty: no task points"

    first_wp = waypoints[0]
    pose = {"x": float(first_wp["x"]), "y": float(first_wp["y"]), "yaw": float(first_wp["yaw"])}
    q = yaw_to_quaternion(pose["yaw"])
    assert abs(quat_norm(q) - 1.0) < 1.0e-9, "simulated pose quaternion is not normalized"

    matched = match_pose_to_start_node(topology, waypoint_index, waypoints, pose)
    assert matched["start_node"] == "N0001", f"unexpected current-pose start: {matched}"

    explicit_route = dijkstra(topology, "N0001", "N0003")
    assert explicit_route, "N0001 -> N0003 should produce a non-empty route"
    inferred_route = dijkstra(topology, matched["start_node"], "N0003")
    assert inferred_route, f"{matched['start_node']} -> N0003 should produce a non-empty route"
    trajectory = stitch(inferred_route, waypoint_index, waypoints)
    assert len(trajectory) > 3, "stitched trajectory too short"

    replay_a = trajectory[0]
    replay_b = trajectory[min(3, len(trajectory) - 1)]
    assert math.hypot(float(replay_b["x"]) - float(replay_a["x"]), float(replay_b["y"]) - float(replay_a["y"])) > 0.0

    stale_failure = "current localization pose is stale"
    missing_pose_failure = "current localization pose is not available"
    print(
        "Offline simulation smoke OK: "
        f"nodes={len(topology['nodes'])}, edges={len(topology['edges'])}, "
        f"waypoints={len(waypoints)}, areas={len(areas)}, task_points={len(task_points)}, "
        f"matched_start={matched}, route={inferred_route}, trajectory_points={len(trajectory)}, "
        f"expected_failures=[{missing_pose_failure!r}, {stale_failure!r}]"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
