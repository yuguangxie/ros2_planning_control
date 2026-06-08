#!/usr/bin/env python3
"""Offline planning/control smoke test without ROS2."""
from __future__ import annotations
import argparse, heapq, json, math
from pathlib import Path

try:
    import yaml
except Exception:  # pragma: no cover
    yaml = None


def load_json(p: Path):
    return json.loads(p.read_text(encoding="utf-8"))


def load_yaml(p: Path):
    if yaml is None:
        raise RuntimeError("PyYAML required")
    return yaml.safe_load(p.read_text(encoding="utf-8"))


def dijkstra(nodes, edges, start, goal):
    adj = {}
    for e in edges:
        adj.setdefault(e["from"], []).append(e)
    pq = [(0.0, start, [])]
    seen = {}
    while pq:
        cost, node, path = heapq.heappop(pq)
        if node in seen and seen[node] <= cost:
            continue
        seen[node] = cost
        if node == goal:
            return path
        for e in adj.get(node, []):
            if not e.get("availability", {}).get("enabled", True):
                continue
            heapq.heappush(pq, (cost + float(e.get("cost", 1.0)), e["to"], path + [e["id"]]))
    return []


def stitch(edge_ids, index, waypoints):
    out = []
    for edge_id in edge_ids:
        rng = index["edges"][edge_id]
        start = int(rng["start_index"])
        end = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
        part = waypoints[start:end]
        if out and part and out[-1].get("waypoint_id") == part[0].get("waypoint_id"):
            part = part[1:]
        out.extend(part)
    # regenerate route s
    route_s = 0.0
    prev = None
    for wp in out:
        if prev is not None:
            route_s += math.hypot(float(wp["x"]) - float(prev["x"]), float(wp["y"]) - float(prev["y"]))
        wp["route_s_m"] = route_s
        prev = wp
    return out


def pure_pursuit(pose, traj, wheel_base=1.2, lookahead=1.0):
    px, py, yaw = pose
    nearest = min(traj, key=lambda w: math.hypot(float(w["x"]) - px, float(w["y"]) - py))
    target_s = nearest["route_s_m"] + lookahead
    target = traj[-1]
    for wp in traj:
        if wp["route_s_m"] >= target_s:
            target = wp
            break
    dx = float(target["x"]) - px
    dy = float(target["y"]) - py
    # map to vehicle frame
    y_vehicle = -math.sin(yaw) * dx + math.cos(yaw) * dy
    kappa = 2.0 * y_vehicle / max(lookahead * lookahead, 1e-6)
    steer = math.atan(wheel_base * kappa)
    return float(target["v_mps"]), steer


def stanley(pose, traj, speed=0.5, k=0.8):
    px, py, yaw = pose
    nearest = min(traj, key=lambda w: math.hypot(float(w["x"]) - px, float(w["y"]) - py))
    heading_error = math.atan2(math.sin(float(nearest["yaw"]) - yaw), math.cos(float(nearest["yaw"]) - yaw))
    dx = px - float(nearest["x"])
    dy = py - float(nearest["y"])
    lateral = -math.sin(float(nearest["yaw"])) * dx + math.cos(float(nearest["yaw"])) * dy
    steer = heading_error + math.atan2(k * lateral, abs(speed) + 0.1)
    return float(nearest["v_mps"]), steer


def front_ackermann(kappa, wheel_base=1.2):
    front = math.atan(kappa * wheel_base)
    return front, 0.0


def dual_ackermann(kappa, wheel_base=1.2, rear_steer_ratio=0.5):
    tan_front = kappa * wheel_base / (1.0 + rear_steer_ratio)
    front = math.atan(tan_front)
    rear = math.atan(-rear_steer_ratio * tan_front)
    return front, rear


def controlled_stop(reason):
    return {
        "speed_mps": 0.0,
        "front_steering_angle_rad": 0.0,
        "rear_steering_angle_rad": 0.0,
        "enable": False,
        "emergency_stop": True,
        "reason": reason,
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("package", nargs="?", default="src/low_speed_av_bringup/sample_ad_package")
    args = ap.parse_args()
    root = Path(args.package)
    manifest = load_json(root / "project_manifest.json")
    topology = load_json(root / manifest["files"].get("topology", "roadnet/topology.json"))
    parking = load_json(root / manifest["files"].get("parking_points", "semantics/parking_points.json"))
    waypoints = load_yaml(root / manifest["files"].get("waypoints_yaml", "trajectory/waypoints.yaml"))["waypoints"]
    index = load_json(root / manifest["files"].get("waypoint_index", "trajectory/waypoint_index.json"))
    parking_goal = parking["parking_points"][0]
    assert parking_goal.get("linked_edge_id") == "E_L002_F", "parking goal link changed"
    edge_ids = dijkstra(topology["nodes"], topology["edges"], "N0001", "N0003")
    assert edge_ids, "no route found"
    traj = stitch(edge_ids, index, waypoints)
    assert len(traj) >= 3, "trajectory too short"
    pp_speed, pp_steer = pure_pursuit((0.0, 0.0, 0.0), traj)
    st_speed, st_steer = stanley((0.0, 0.0, 0.0), traj)
    for name, value in [("pp_speed", pp_speed), ("pp_steer", pp_steer), ("st_speed", st_speed), ("st_steer", st_steer)]:
        assert math.isfinite(value), f"{name} not finite"
    assert abs(pp_steer) <= 0.7, "pure pursuit steer exceeds demo limit"
    assert abs(st_steer) <= 0.7, "stanley steer exceeds demo limit"
    for model_name, values in [
        ("front_ackermann", front_ackermann(math.tan(pp_steer) / 1.2)),
        ("dual_ackermann", dual_ackermann(math.tan(pp_steer) / 1.2)),
    ]:
        for value in values:
            assert math.isfinite(value), f"{model_name} steering not finite"
            assert abs(value) <= 0.7, f"{model_name} steering exceeds demo limit"
    estop = controlled_stop("safety_estop")
    assert estop["emergency_stop"] and not estop["enable"] and estop["reason"] == "safety_estop"
    print(f"Offline algorithm smoke OK: route={edge_ids}, traj_points={len(traj)}, pp=({pp_speed:.3f},{pp_steer:.3f}), stanley=({st_speed:.3f},{st_steer:.3f}), ackermann=finite, estop=ok")

if __name__ == "__main__":
    main()
