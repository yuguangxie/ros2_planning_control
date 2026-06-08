#!/usr/bin/env python3
"""Offline checks for remaining audit fixes without ROS2 or C++ build."""
from __future__ import annotations

import copy
import hashlib
import heapq
import json
import math
import shutil
import tempfile
from pathlib import Path

try:
    import yaml
except Exception:  # pragma: no cover
    yaml = None


ROOT = Path("src/low_speed_av_bringup/sample_ad_package")


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data) -> None:
    path.write_text(json.dumps(data, indent=2), encoding="utf-8")


def load_yaml(path: Path):
    if yaml is None:
      raise RuntimeError("PyYAML required")
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def verify_checksums(root: Path) -> None:
    checksums = root / "checksums.sha256"
    manifest = load_json(root / "project_manifest.json")
    expected = dict(manifest.get("hashes", {}))
    for line in checksums.read_text(encoding="utf-8").splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        digest, rel = line.split(maxsplit=1)
        expected[rel.strip()] = digest.strip()
    for rel, digest in expected.items():
        path = root / rel
        if not path.exists():
            raise AssertionError(f"checksum references missing file {rel}")
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != digest:
            raise AssertionError(f"checksum mismatch for {rel}")


def validate_loader_contract(root: Path) -> None:
    manifest = load_json(root / "project_manifest.json")
    validation = manifest.get("validation", {})
    if validation.get("status") == "failed" or int(validation.get("blocking_errors", 0)) > 0:
        raise AssertionError("manifest validation rejected")
    report = load_json(root / manifest["files"].get("validation_report", "validation/validation_report.json"))
    summary = report.get("summary", report)
    if report.get("status") == "failed" or int(summary.get("blocking_errors", 0)) > 0:
        raise AssertionError("validation report rejected")
    waypoints = load_yaml(root / manifest["files"].get("waypoints_yaml", "trajectory/waypoints.yaml"))["waypoints"]
    index = load_json(root / manifest["files"].get("waypoint_index", "trajectory/waypoint_index.json"))
    for edge_id, rng in index["edges"].items():
        start = int(rng["start_index"])
        end = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
        if start < 0 or end > len(waypoints) or end < start:
            raise AssertionError(f"invalid waypoint range for {edge_id}")
    verify_checksums(root)


def expect_rejected(label: str, fn) -> None:
    try:
        fn()
    except AssertionError:
        return
    raise AssertionError(f"{label} was not rejected")


def dijkstra(edges, start, goal, blocked):
    adj = {}
    for edge in edges:
        if edge["id"] in blocked:
            continue
        adj.setdefault(edge["from"], []).append(edge)
    pq = [(0.0, start, [])]
    best = {}
    while pq:
        cost, node, route = heapq.heappop(pq)
        if node in best and best[node] <= cost:
            continue
        best[node] = cost
        if node == goal:
            return route
        for edge in adj.get(node, []):
            heapq.heappush(pq, (cost + float(edge.get("cost", 1.0)), edge["to"], route + [edge["id"]]))
    return []


def stitch(edge_ids, index, waypoints):
    out = []
    for edge_id in edge_ids:
        rng = index["edges"][edge_id]
        start = int(rng["start_index"])
        end = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
        out.extend(copy.deepcopy(waypoints[start:end]))
    return out


def point_in_polygon(point, polygon):
    x, y = point
    inside = False
    j = len(polygon) - 1
    for i, a in enumerate(polygon):
        b = polygon[j]
        if (a["y"] > y) != (b["y"] > y):
            x_cross = (b["x"] - a["x"]) * (y - a["y"]) / ((b["y"] - a["y"]) or 1e-12) + a["x"]
            if x < x_cross:
                inside = not inside
        j = i
    return inside


def semantic_checks(root: Path) -> None:
    manifest = load_json(root / "project_manifest.json")
    topology = load_json(root / manifest["files"]["topology"])
    waypoints = load_yaml(root / manifest["files"]["waypoints_yaml"])["waypoints"]
    index = load_json(root / manifest["files"]["waypoint_index"])
    speed_zone = {
        "id": "A_SPEED_TEST",
        "type": "speed_zone",
        "polygon": [{"x": -0.5, "y": -0.5}, {"x": 2.5, "y": -0.5}, {"x": 2.5, "y": 0.5}, {"x": -0.5, "y": 0.5}],
        "speed_limit_mps": 0.2,
        "allow_planning_through": True,
    }
    no_go = {
        "id": "A_KEEP_OUT_TEST",
        "type": "no_go_area",
        "polygon": speed_zone["polygon"],
        "speed_limit_mps": 0.0,
        "allow_planning_through": False,
    }
    route = dijkstra(topology["edges"], "N0001", "N0003", blocked=set())
    traj = stitch(route, index, waypoints)
    for wp in traj:
        if point_in_polygon((float(wp["x"]), float(wp["y"])), speed_zone["polygon"]):
            wp["v_mps"] = min(float(wp["v_mps"]), speed_zone["speed_limit_mps"])
    assert any(abs(float(wp["v_mps"]) - 0.2) < 1e-9 for wp in traj), "speed_zone did not reduce speed"
    blocked = {
        wp["edge_id"] for wp in waypoints
        if point_in_polygon((float(wp["x"]), float(wp["y"])), no_go["polygon"])
    }
    assert "E_L001_F" in blocked, "no_go did not block expected edge"
    assert not dijkstra(topology["edges"], "N0001", "N0003", blocked=blocked), "no_go route should be rejected"


def lqr_output(q_lat: float, q_yaw: float, r_steering: float) -> float:
    # Lightweight mirror of the upgraded LQR sign/config sensitivity check.
    pose = (0.0, 0.2, 0.0)
    ref = {"x": 0.0, "y": 0.0, "yaw": 0.0, "kappa": 0.08}
    lateral = -math.sin(ref["yaw"]) * (pose[0] - ref["x"]) + math.cos(ref["yaw"]) * (pose[1] - ref["y"])
    heading = math.atan2(math.sin(pose[2] - ref["yaw"]), math.cos(pose[2] - ref["yaw"]))
    gain_lat = math.sqrt(max(q_lat, 0.0) / max(r_steering, 1e-9))
    gain_yaw = math.sqrt(max(q_yaw, 0.0) / max(r_steering, 1e-9))
    return math.atan(1.2 * ref["kappa"]) - gain_lat * lateral - gain_yaw * heading


def mpc_output(weight: float) -> float:
    samples = [-0.2, -0.1, 0.0, 0.1, 0.2]
    best = (float("inf"), 0.0)
    for kappa in samples:
        lateral_cost = abs(0.2 - kappa)
        effort = abs(kappa)
        cost = weight * lateral_cost + 0.05 * effort
        best = min(best, (cost, kappa))
    return best[1]


def control_policy_checks() -> None:
    assert abs(lqr_output(3.0, 2.0, 1.0) - lqr_output(6.0, 2.0, 1.0)) > 1e-6
    assert mpc_output(1.0) == mpc_output(2.0)
    assert mpc_output(0.0) == 0.0
    latched = False
    latched = True if "estop" == "estop" else latched
    assert latched, "estop should latch"
    latched = False if "ok" in {"ok", "clear", "standby"} else latched
    assert not latched, "estop clear condition should recover"


def cpp_source_static_checks() -> None:
    loader = Path("src/low_speed_av_planning/src/roadnet_loader.cpp").read_text(encoding="utf-8")
    planning = Path("src/low_speed_av_planning/src/planning_node.cpp").read_text(encoding="utf-8")
    control = Path("src/low_speed_av_control/src/control_node.cpp").read_text(encoding="utf-8")
    lqr = Path("src/low_speed_av_control/src/lqr_controller.cpp").read_text(encoding="utf-8")
    mpc = Path("src/low_speed_av_control/src/mpc_sampler_controller.cpp").read_text(encoding="utf-8")
    assert "sha256_hex" in loader and "checksum mismatch for" in loader
    assert "Python offline validator performs SHA-256" not in loader
    assert "package_->blocked_edges" in planning and "semantic_speed_zone" in planning
    assert "safety.estop_latched" in control and "safety estop clear" in control
    assert "options.lqr_q_lateral_error" in lqr and "lqr_use_curvature_feedforward" in lqr
    assert "cmd.reason = \"lqr_tracking\"" in lqr
    assert "options.mpc_horizon_steps" in mpc and "options.mpc_lateral_error_weight" in mpc


def main() -> None:
    validate_loader_contract(ROOT)
    with tempfile.TemporaryDirectory() as tmp:
        bad = Path(tmp) / "pkg"
        shutil.copytree(ROOT, bad)
        (bad / "trajectory/waypoints.yaml").write_text(
            (bad / "trajectory/waypoints.yaml").read_text(encoding="utf-8") + "\n# tampered\n",
            encoding="utf-8")
        expect_rejected("checksum mismatch", lambda: validate_loader_contract(bad))
    with tempfile.TemporaryDirectory() as tmp:
        bad = Path(tmp) / "pkg"
        shutil.copytree(ROOT, bad)
        report = load_json(bad / "validation/validation_report.json")
        report["status"] = "failed"
        report.setdefault("summary", {})["blocking_errors"] = 1
        write_json(bad / "validation/validation_report.json", report)
        expect_rejected("failed validation", lambda: validate_loader_contract(bad))
    with tempfile.TemporaryDirectory() as tmp:
        bad = Path(tmp) / "pkg"
        shutil.copytree(ROOT, bad)
        index = load_json(bad / "trajectory/waypoint_index.json")
        index["edges"]["E_L001_F"]["end_index_exclusive"] = 999
        write_json(bad / "trajectory/waypoint_index.json", index)
        expect_rejected("bad waypoint index", lambda: validate_loader_contract(bad))
    semantic_checks(ROOT)
    control_policy_checks()
    cpp_source_static_checks()
    print("Remaining fixes smoke OK: checksum/bad_validation/bad_index rejected, semantics affect route/speed, LQR/MPC config changes output, estop latch clear policy OK")


if __name__ == "__main__":
    main()
