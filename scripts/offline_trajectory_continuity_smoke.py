#!/usr/bin/env python3
"""Offline smoke checks for semantic-goal trajectory continuity.

The script mirrors the planner's source-level contracts without importing ROS2:
semantic targets are resolved to edge anchors, a full reference path is built
before the local horizon crop, and the control trajectory must not jump from a
near horizon point to a remote semantic target edge.
"""

from __future__ import annotations

import heapq
import json
import math
from dataclasses import dataclass
from pathlib import Path

try:
    import yaml
except ImportError as exc:  # pragma: no cover - environment diagnostic
    raise SystemExit(f"PyYAML is required: {exc}") from exc


MAX_JUMP_M = 2.0
HORIZON_M = 15.0
CURRENT_POSE = {"x": 0.554, "y": 1.473, "yaw": -0.9178}


@dataclass
class Anchor:
    kind: str
    point_id: str
    node_id: str
    edge_id: str = ""
    edge_from: str = ""
    edge_to: str = ""
    waypoint_index: int = 0
    x: float = 0.0
    y: float = 0.0
    yaw: float = 0.0
    require_stop: bool = True


def clean_optional(value) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    return "" if text == "" or text.lower() in {"null", "none"} else text


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_yaml(path: Path):
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def load_package(root: Path) -> dict:
    manifest = load_json(root / "project_manifest.json")
    topology = load_json(root / manifest["files"].get("topology", "roadnet/topology.json"))
    waypoint_index = load_json(root / manifest["files"].get("waypoint_index", "trajectory/waypoint_index.json"))
    waypoints = load_yaml(root / manifest["files"].get("waypoints_yaml", "trajectory/waypoints.yaml"))["waypoints"]

    def semantic(name: str) -> dict:
        path = root / manifest["files"].get(name, f"semantics/{name}.json")
        data = load_json(path)
        return {item["id"]: item for item in data.get(name, [])}

    edges = {edge["id"]: edge for edge in topology["edges"]}
    return {
        "root": root,
        "edges": edges,
        "nodes": {node["id"]: node for node in topology["nodes"]},
        "waypoint_index": waypoint_index["edges"],
        "waypoints": waypoints,
        "task_points": semantic("task_points"),
        "parking_points": semantic("parking_points"),
        "charging_points": semantic("charging_points"),
    }


def edge_range(package: dict, edge_id: str) -> tuple[int, int]:
    entry = package["waypoint_index"][edge_id]
    start = int(entry["start_index"])
    if "end_index_exclusive" in entry:
        return start, int(entry["end_index_exclusive"])
    return start, int(entry["end_index"]) + 1


def edge_from(edge: dict) -> str:
    return edge.get("from") or edge.get("from_node_id")


def edge_to(edge: dict) -> str:
    return edge.get("to") or edge.get("to_node_id")


def normalize_angle(angle: float) -> float:
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def dijkstra(package: dict, start: str, goal: str) -> tuple[list[str], list[str]]:
    if start not in package["nodes"] or goal not in package["nodes"]:
        return [], []
    adjacency: dict[str, list[dict]] = {}
    for edge in package["edges"].values():
        adjacency.setdefault(edge_from(edge), []).append(edge)

    queue = [(0.0, start, [start], [])]
    best: dict[str, float] = {}
    while queue:
        cost, node, nodes, edges = heapq.heappop(queue)
        if node in best and best[node] <= cost:
            continue
        best[node] = cost
        if node == goal:
            return nodes, edges
        for edge in adjacency.get(node, []):
            if not edge.get("availability", {}).get("enabled", True):
                continue
            nxt = edge_to(edge)
            heapq.heappush(
                queue,
                (cost + float(edge.get("cost", edge.get("length_m", 1.0))), nxt, nodes + [nxt], edges + [edge["id"]]),
            )
    return [], []


def semantic_edge_id(point: dict) -> str:
    edge_id = clean_optional(point.get("linked_edge_id"))
    if not edge_id:
        edge_id = clean_optional(point.get("entry_edge_id"))
    if not edge_id:
        approach = point.get("approach")
        if isinstance(approach, dict):
            edge_id = clean_optional(approach.get("edge_id"))
    return edge_id


def semantic_anchor(package: dict, collection: str, point_id: str) -> Anchor:
    points = package[collection]
    if point_id not in points:
        raise AssertionError(f"{collection[:-1].replace('_', ' ')} not found: {point_id}")
    point = points[point_id]
    edge_id = semantic_edge_id(point)
    if edge_id not in package["edges"]:
        raise AssertionError(f"{point_id} has invalid linked_edge_id: {edge_id}")
    edge = package["edges"][edge_id]
    start, end = edge_range(package, edge_id)
    px = float(point["pose"]["x"])
    py = float(point["pose"]["y"])
    best_index = min(
        range(start, end),
        key=lambda i: math.hypot(float(package["waypoints"][i]["x"]) - px, float(package["waypoints"][i]["y"]) - py),
    )
    kind = collection.removesuffix("s")
    return Anchor(
        kind=kind,
        point_id=point_id,
        node_id=edge_to(edge),
        edge_id=edge_id,
        edge_from=edge_from(edge),
        edge_to=edge_to(edge),
        waypoint_index=best_index,
        x=px,
        y=py,
        yaw=float(point["pose"].get("yaw", 0.0)),
        require_stop=True,
    )


def node_anchor(package: dict, node_id: str) -> Anchor:
    node = package["nodes"][node_id]
    pose = node.get("pose", {})
    return Anchor(
        kind="node",
        point_id=node_id,
        node_id=node_id,
        x=float(pose.get("x", 0.0)),
        y=float(pose.get("y", 0.0)),
        yaw=float(pose.get("yaw", 0.0)),
        require_stop=False,
    )


def current_pose_anchor(package: dict) -> Anchor:
    best_i = 0
    best_dist = float("inf")
    for i, wp in enumerate(package["waypoints"]):
        if abs(normalize_angle(CURRENT_POSE["yaw"] - float(wp["yaw"]))) > 1.57:
            continue
        d = math.hypot(CURRENT_POSE["x"] - float(wp["x"]), CURRENT_POSE["y"] - float(wp["y"]))
        if d < best_dist:
            best_i = i
            best_dist = d
    if best_dist > 3.0:
        raise AssertionError(f"current pose too far from roadnet: {best_dist:.3f} m")
    wp = package["waypoints"][best_i]
    edge = package["edges"][wp["edge_id"]]
    start, end = edge_range(package, wp["edge_id"])
    progress = (best_i - start) / max(1, end - start - 1)
    return Anchor(
        kind="current_pose",
        point_id="current_pose",
        node_id=edge_to(edge) if progress >= 0.5 else edge_from(edge),
        edge_id=wp["edge_id"],
        edge_from=edge_from(edge),
        edge_to=edge_to(edge),
        waypoint_index=best_i,
        x=CURRENT_POSE["x"],
        y=CURRENT_POSE["y"],
        yaw=CURRENT_POSE["yaw"],
        require_stop=False,
    )


def stitch(package: dict, edge_ids: list[str]) -> list[dict]:
    out: list[dict] = []
    for edge_id in edge_ids:
        start, end = edge_range(package, edge_id)
        part = [dict(package["waypoints"][i]) for i in range(start, end)]
        if out and part and out[-1].get("waypoint_id") == part[0].get("waypoint_id"):
            part = part[1:]
        out.extend(part)
    regenerate_s(out)
    return out


def append_goal_segment(package: dict, full: list[dict], goal: Anchor, reverse: bool) -> None:
    start, end = edge_range(package, goal.edge_id)
    if reverse:
        indices = range(end - 1, goal.waypoint_index - 1, -1)
        gear = 2
    else:
        indices = range(start, goal.waypoint_index + 1)
        gear = 1
    for i in indices:
        wp = dict(package["waypoints"][i])
        wp["gear"] = gear
        if not full or math.hypot(float(full[-1]["x"]) - float(wp["x"]), float(full[-1]["y"]) - float(wp["y"])) > 1.0e-4:
            full.append(wp)
    if full:
        full[-1]["waypoint_id"] = goal.point_id
        full[-1]["x"] = goal.x
        full[-1]["y"] = goal.y
        full[-1]["yaw"] = goal.yaw
        full[-1]["edge_id"] = goal.edge_id
        full[-1]["v_mps"] = 0.0
        full[-1]["behavior"] = "semantic_goal_stop"
    regenerate_s(full)


def regenerate_s(points: list[dict]) -> None:
    s = 0.0
    prev = None
    for wp in points:
        if prev is not None:
            s += math.hypot(float(wp["x"]) - float(prev["x"]), float(wp["y"]) - float(prev["y"]))
        wp["route_s_m"] = s
        prev = wp


def build_full_reference(package: dict, start: Anchor, goal: Anchor) -> tuple[list[dict], list[str], list[str]]:
    if start.node_id == goal.node_id and math.hypot(start.x - goal.x, start.y - goal.y) <= 0.5:
        return [{"waypoint_id": goal.point_id, "edge_id": goal.edge_id, "x": goal.x, "y": goal.y, "yaw": goal.yaw, "route_s_m": 0.0}], [start.node_id], []

    allow_reverse = True
    if goal.edge_id and not (allow_reverse and start.node_id == goal.edge_to):
        target_node = goal.edge_from
    else:
        target_node = goal.node_id
    nodes, edges = dijkstra(package, start.node_id, target_node)
    if not nodes and target_node != goal.node_id:
        nodes, edges = dijkstra(package, start.node_id, goal.node_id)
    if not nodes:
        raise AssertionError(f"no route from {start.node_id} to {target_node}")
    full = stitch(package, edges)
    if goal.edge_id:
        reverse = (nodes[-1] == goal.edge_to and allow_reverse) or (not edges and start.node_id == goal.edge_to)
        append_goal_segment(package, full, goal, reverse)
    return full, nodes, edges


def local_from_full(package: dict, full: list[dict]) -> list[dict]:
    current = current_pose_anchor(package)
    start_index = min(
        range(len(full)),
        key=lambda i: math.hypot(current.x - float(full[i]["x"]), current.y - float(full[i]["y"])),
    )
    start_s = float(full[start_index].get("route_s_m", 0.0))
    local: list[dict] = []
    for wp in full[start_index:]:
        if local and float(wp.get("route_s_m", 0.0)) - start_s > HORIZON_M:
            break
        local.append(dict(wp))
    regenerate_s(local)
    return local


def assert_continuous(points: list[dict], label: str) -> None:
    assert points, f"{label}: trajectory is empty"
    known_waypoint_ids = {wp.get("waypoint_id") for wp in points if str(wp.get("waypoint_id", "")).startswith("WP_")}
    assert known_waypoint_ids or len(points) == 1, f"{label}: no normal waypoint ids found"
    for i in range(1, len(points)):
        jump = math.hypot(float(points[i]["x"]) - float(points[i - 1]["x"]), float(points[i]["y"]) - float(points[i - 1]["y"]))
        assert jump <= MAX_JUMP_M, f"{label}: jump {jump:.3f} m at {i} from {points[i-1].get('edge_id')} to {points[i].get('edge_id')}"


def run_case(package: dict, label: str, start: Anchor, goal: Anchor, use_current_local: bool) -> None:
    full, nodes, edges = build_full_reference(package, start, goal)
    assert_continuous(full, label + " full")
    local = local_from_full(package, full) if use_current_local else full[:]
    assert_continuous(local, label + " local")
    if goal.edge_id:
        assert full[-1].get("waypoint_id") == goal.point_id, f"{label}: full reference does not end at semantic target"
    print(f"  PASS {label}: nodes={nodes}, edges={edges}, full_points={len(full)}, local_points={len(local)}")


def assert_config_contracts() -> None:
    planning = Path("src/low_speed_av_bringup/config/planning_params.yaml").read_text(encoding="utf-8")
    control = Path("src/low_speed_av_bringup/config/control_params.yaml").read_text(encoding="utf-8")
    assert "trajectory_republish_rate_hz: 10.0" in planning
    assert "publish_full_reference_path: true" in planning
    assert 'full_reference_path_topic: "/planning/full_reference_path"' in planning
    assert "max_trajectory_point_jump_m: 2.0" in planning
    assert "status_publish_rate_hz: 5.0" in control
    assert "max_steering_angle_deg: 27.0" in control
    assert 'overrange_policy: "clamp"' in control


def main() -> int:
    roadnet_a = load_package(Path("roadnet_ad_package_20260610T012525Z_1"))
    roadnet_b = load_package(Path("roadnet_ad_package_20260610T012525Z_2"))

    run_case(roadnet_a, "Roadnet A current_pose -> RP-001", current_pose_anchor(roadnet_a), semantic_anchor(roadnet_a, "task_points", "RP-001"), True)
    run_case(roadnet_a, "Roadnet A RP-003 -> RP-001", semantic_anchor(roadnet_a, "task_points", "RP-003"), semantic_anchor(roadnet_a, "task_points", "RP-001"), False)
    run_case(roadnet_a, "Roadnet A current_pose -> RP-008", current_pose_anchor(roadnet_a), semantic_anchor(roadnet_a, "task_points", "RP-008"), True)
    run_case(roadnet_a, "Roadnet A current_pose -> charging RP-017", current_pose_anchor(roadnet_a), semantic_anchor(roadnet_a, "charging_points", "RP-017"), True)
    try:
        semantic_anchor(roadnet_a, "charging_points", "BAD_CHARGING")
    except AssertionError as exc:
        assert "not found: BAD_CHARGING" in str(exc)
    else:
        raise AssertionError("invalid charging target should fail")

    run_case(roadnet_b, "Roadnet B current_pose -> RP-003", current_pose_anchor(roadnet_b), semantic_anchor(roadnet_b, "task_points", "RP-003"), True)
    run_case(roadnet_b, "Roadnet B current_pose -> RP-001", current_pose_anchor(roadnet_b), semantic_anchor(roadnet_b, "task_points", "RP-001"), True)
    run_case(roadnet_b, "Roadnet B N0015 -> N0014", node_anchor(roadnet_b, "N0015"), node_anchor(roadnet_b, "N0014"), False)

    assert_config_contracts()
    print("offline_trajectory_continuity_smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
