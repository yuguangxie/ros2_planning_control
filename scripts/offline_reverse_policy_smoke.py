#!/usr/bin/env python3
"""Offline checks for reverse planning policy and current-pose start anchors.

This script intentionally uses only JSON/CSV from the AD Package so it can run
in lean Windows Codex environments where PyYAML is not installed.
"""

from __future__ import annotations

import csv
import heapq
import json
import math
from dataclasses import dataclass
from pathlib import Path


MAX_FIRST_POINT_DISTANCE_M = 2.0
MAX_JUMP_M = 2.0
HORIZON_M = 15.0
ROADNET_A_CURRENT_WAYPOINT = "WP_E_C-012_F_000039"
ROADNET_B_CURRENT_POSE = {"x": 0.554, "y": 1.473, "yaw": -0.9178}


@dataclass
class Anchor:
    kind: str
    point_id: str
    node_id: str
    edge_id: str = ""
    edge_from: str = ""
    edge_to: str = ""
    waypoint_index: int = 0
    s_on_edge: float = 0.0
    x: float = 0.0
    y: float = 0.0
    yaw: float = 0.0


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def clean_optional(value) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    return "" if text == "" or text.lower() in {"null", "none"} else text


def load_package(root: Path) -> dict:
    manifest = load_json(root / "project_manifest.json")
    topology = load_json(root / manifest["files"].get("topology", "roadnet/topology.json"))
    waypoint_index = load_json(root / manifest["files"].get("waypoint_index", "trajectory/waypoint_index.json"))
    with (root / manifest["files"].get("waypoints_csv", "trajectory/waypoints.csv")).open(
        newline="", encoding="utf-8"
    ) as handle:
        waypoints = list(csv.DictReader(handle))

    def semantic(name: str) -> dict:
        path = root / manifest["files"].get(name, f"semantics/{name}.json")
        data = load_json(path)
        return {item["id"]: item for item in data.get(name, [])}

    return {
        "root": root,
        "nodes": {node["id"]: node for node in topology["nodes"]},
        "edges": {edge["id"]: edge for edge in topology["edges"]},
        "waypoint_index": waypoint_index["edges"],
        "waypoints": waypoints,
        "task_points": semantic("task_points"),
        "parking_points": semantic("parking_points"),
        "charging_points": semantic("charging_points"),
    }


def edge_from(edge: dict) -> str:
    return edge.get("from") or edge.get("from_node_id")


def edge_to(edge: dict) -> str:
    return edge.get("to") or edge.get("to_node_id")


def edge_direction(edge: dict) -> str:
    return str(edge.get("direction", "")).lower()


def edge_range(package: dict, edge_id: str) -> tuple[int, int]:
    entry = package["waypoint_index"][edge_id]
    start = int(entry["start_index"])
    if "end_index_exclusive" in entry:
        return start, int(entry["end_index_exclusive"])
    return start, int(entry["end_index"]) + 1


def normalize_angle(angle: float) -> float:
    while angle > math.pi:
        angle -= 2.0 * math.pi
    while angle < -math.pi:
        angle += 2.0 * math.pi
    return angle


def dist(a: dict | Anchor, b: dict | Anchor) -> float:
    ax = float(a.x if isinstance(a, Anchor) else a["x"])
    ay = float(a.y if isinstance(a, Anchor) else a["y"])
    bx = float(b.x if isinstance(b, Anchor) else b["x"])
    by = float(b.y if isinstance(b, Anchor) else b["y"])
    return math.hypot(ax - bx, ay - by)


def semantic_edge_id(point: dict) -> str:
    edge_id = clean_optional(point.get("linked_edge_id"))
    if not edge_id:
        edge_id = clean_optional(point.get("entry_edge_id"))
    if not edge_id and isinstance(point.get("approach"), dict):
        edge_id = clean_optional(point["approach"].get("edge_id"))
    return edge_id


def semantic_anchor(package: dict, collection: str, point_id: str) -> Anchor:
    point = package[collection].get(point_id)
    if not point:
        raise AssertionError(f"{collection[:-1].replace('_', ' ')} not found: {point_id}")
    edge_id = semantic_edge_id(point)
    if edge_id not in package["edges"]:
        raise AssertionError(f"{point_id} has invalid linked_edge_id: {edge_id}")
    edge = package["edges"][edge_id]
    px = float(point["pose"]["x"])
    py = float(point["pose"]["y"])
    start, end = edge_range(package, edge_id)
    best = min(
        range(start, end),
        key=lambda i: math.hypot(float(package["waypoints"][i]["x"]) - px, float(package["waypoints"][i]["y"]) - py),
    )
    return Anchor(
        kind=collection.removesuffix("s"),
        point_id=point_id,
        node_id=edge_to(edge),
        edge_id=edge_id,
        edge_from=edge_from(edge),
        edge_to=edge_to(edge),
        waypoint_index=best,
        s_on_edge=float(package["waypoints"][best].get("s_m", 0.0)),
        x=px,
        y=py,
        yaw=float(point["pose"].get("yaw", 0.0)),
    )


def current_anchor_from_waypoint(package: dict, waypoint_id: str) -> Anchor:
    for i, wp in enumerate(package["waypoints"]):
        if wp["waypoint_id"] == waypoint_id:
            edge = package["edges"][wp["edge_id"]]
            return Anchor(
                kind="current_pose",
                point_id="current_pose",
                node_id=edge_to(edge),
                edge_id=wp["edge_id"],
                edge_from=edge_from(edge),
                edge_to=edge_to(edge),
                waypoint_index=i,
                s_on_edge=float(wp.get("s_m", 0.0)),
                x=float(wp["x"]),
                y=float(wp["y"]),
                yaw=float(wp["yaw"]),
            )
    raise AssertionError(f"waypoint not found: {waypoint_id}")


def current_anchor_from_pose(package: dict, pose: dict) -> Anchor:
    best = min(
        range(len(package["waypoints"])),
        key=lambda i: (
            abs(normalize_angle(float(package["waypoints"][i]["yaw"]) - pose["yaw"])) > 1.57,
            math.hypot(float(package["waypoints"][i]["x"]) - pose["x"], float(package["waypoints"][i]["y"]) - pose["y"]),
        ),
    )
    wp = package["waypoints"][best]
    edge = package["edges"][wp["edge_id"]]
    return Anchor(
        kind="current_pose",
        point_id="current_pose",
        node_id=edge_to(edge),
        edge_id=wp["edge_id"],
        edge_from=edge_from(edge),
        edge_to=edge_to(edge),
        waypoint_index=best,
        s_on_edge=float(wp.get("s_m", 0.0)),
        x=pose["x"],
        y=pose["y"],
        yaw=pose["yaw"],
    )


def dijkstra(package: dict, start: str, goal: str, allow_reverse: bool) -> tuple[list[str], list[str]]:
    if start not in package["nodes"] or goal not in package["nodes"]:
        return [], []
    adjacency: dict[str, list[dict]] = {}
    for edge in package["edges"].values():
        if not allow_reverse and edge_direction(edge) == "reverse":
            continue
        adjacency.setdefault(edge_from(edge), []).append(edge)

    queue = [(0.0, start, [start], [])]
    best_cost: dict[str, float] = {}
    while queue:
        cost, node, nodes, edges = heapq.heappop(queue)
        if node in best_cost and best_cost[node] <= cost:
            continue
        best_cost[node] = cost
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


def append_point(points: list[dict], wp: dict) -> None:
    if points and dist(points[-1], wp) < 1.0e-4:
        return
    points.append(wp)


def append_start_prefix(package: dict, out: list[dict], start: Anchor) -> None:
    edge_start, edge_end = edge_range(package, start.edge_id)
    start_index = min(max(start.waypoint_index, edge_start), edge_end - 1)
    for i in range(start_index, edge_end):
        wp = dict(package["waypoints"][i])
        if i == start_index:
            wp["x"] = start.x
            wp["y"] = start.y
            wp["yaw"] = start.yaw
            wp["s_m"] = start.s_on_edge
        wp["gear"] = 1
        wp["behavior"] = "start_anchor_forward_prefix"
        append_point(out, wp)


def append_stitch(package: dict, out: list[dict], edge_ids: list[str]) -> None:
    for edge_id in edge_ids:
        start, end = edge_range(package, edge_id)
        for i in range(start, end):
            append_point(out, dict(package["waypoints"][i]))


def append_segment_between(package: dict, out: list[dict], start: Anchor, goal: Anchor, reverse: bool) -> None:
    edge_start, edge_end = edge_range(package, goal.edge_id)
    start_index = min(max(start.waypoint_index, edge_start), edge_end - 1)
    goal_index = min(max(goal.waypoint_index, edge_start), edge_end - 1)
    indices = range(start_index, goal_index - 1, -1) if reverse else range(start_index, goal_index + 1)
    for i in indices:
        wp = dict(package["waypoints"][i])
        if i == start_index:
            wp["x"] = start.x
            wp["y"] = start.y
            wp["yaw"] = start.yaw
            wp["s_m"] = start.s_on_edge
        wp["gear"] = 2 if reverse else 1
        wp["behavior"] = "semantic_reverse_local" if reverse else "semantic_forward_local"
        append_point(out, wp)
    if out:
        out[-1]["waypoint_id"] = goal.point_id
        out[-1]["x"] = goal.x
        out[-1]["y"] = goal.y
        out[-1]["yaw"] = goal.yaw
        out[-1]["v_mps"] = 0.0
        out[-1]["behavior"] = "semantic_goal_reverse_stop" if reverse else "semantic_goal_stop"


def append_goal_prefix(package: dict, out: list[dict], goal: Anchor) -> None:
    start_anchor = Anchor(
        kind="edge_start",
        point_id=goal.edge_from,
        node_id=goal.edge_from,
        edge_id=goal.edge_id,
        edge_from=goal.edge_from,
        edge_to=goal.edge_to,
        waypoint_index=edge_range(package, goal.edge_id)[0],
        s_on_edge=0.0,
        x=float(package["waypoints"][edge_range(package, goal.edge_id)[0]]["x"]),
        y=float(package["waypoints"][edge_range(package, goal.edge_id)[0]]["y"]),
        yaw=float(package["waypoints"][edge_range(package, goal.edge_id)[0]]["yaw"]),
    )
    append_segment_between(package, out, start_anchor, goal, reverse=False)


def regenerate_s(points: list[dict]) -> None:
    total = 0.0
    prev = None
    for wp in points:
        if prev is not None:
            total += dist(prev, wp)
        wp["route_s_m"] = total
        prev = wp


def build_reference(
    package: dict,
    start: Anchor,
    goal: Anchor,
    allow_reverse_planning: bool,
    allow_reverse_local: bool,
) -> tuple[bool, str, list[dict], list[str], list[str]]:
    same_edge = start.edge_id and start.edge_id == goal.edge_id
    goal_behind = same_edge and goal.s_on_edge + 1.0e-3 < start.s_on_edge
    full: list[dict] = []

    if same_edge and not goal_behind:
        append_segment_between(package, full, start, goal, reverse=False)
        regenerate_s(full)
        return True, "same edge forward segment selected", full, [start.node_id], []

    if same_edge and goal_behind and allow_reverse_planning and allow_reverse_local:
        append_segment_between(package, full, start, goal, reverse=True)
        regenerate_s(full)
        return True, "reverse local segment selected", full, [start.node_id], []

    target = goal.edge_from if goal.edge_from else goal.node_id
    nodes, edges = dijkstra(package, start.node_id, target, allow_reverse_planning)
    if not nodes:
        return False, "reverse planning is disabled and forward route to goal is unavailable", [], [], []

    append_start_prefix(package, full, start)
    append_stitch(package, full, edges)
    append_goal_prefix(package, full, goal)
    regenerate_s(full)
    note = "reverse disabled; using forward detour" if same_edge and goal_behind else "forward route selected"
    return True, note, full, nodes, edges


def local_from_full(full: list[dict], start: Anchor) -> list[dict]:
    nearest = min(range(len(full)), key=lambda i: dist(full[i], start))
    start_s = float(full[nearest].get("route_s_m", 0.0))
    local: list[dict] = []
    for wp in full[nearest:]:
        if local and float(wp.get("route_s_m", 0.0)) - start_s > HORIZON_M:
            break
        local.append(dict(wp))
    regenerate_s(local)
    return local


def assert_continuous(points: list[dict], label: str) -> None:
    assert points, f"{label}: empty trajectory"
    for i in range(1, len(points)):
        jump = dist(points[i - 1], points[i])
        assert jump <= MAX_JUMP_M, f"{label}: jump {jump:.3f} m at {i}"


def assert_no_reverse(points: list[dict], label: str) -> None:
    assert all(int(wp.get("gear", 1)) != 2 for wp in points), f"{label}: reverse gear found"
    assert not any(str(wp.get("behavior", "")).startswith("semantic_reverse") for wp in points), (
        f"{label}: semantic reverse behavior found"
    )


def assert_config_defaults() -> None:
    planning = Path("src/low_speed_av_bringup/config/planning_params.yaml").read_text(encoding="utf-8")
    assert "allow_reverse_planning: false" in planning
    assert "allow_reverse_local_segment: false" in planning
    assert "include_current_edge_prefix: true" in planning
    assert "max_first_trajectory_point_distance_m: 2.0" in planning


def main() -> int:
    roadnet_a = load_package(Path("roadnet_ad_package_20260610T012525Z_1"))
    roadnet_b = load_package(Path("roadnet_ad_package_20260610T012525Z_2"))

    start_a = current_anchor_from_waypoint(roadnet_a, ROADNET_A_CURRENT_WAYPOINT)
    rp001 = semantic_anchor(roadnet_a, "task_points", "RP-001")
    ok, note, full, _, _ = build_reference(roadnet_a, start_a, rp001, False, False)
    assert ok, note
    local = local_from_full(full, start_a)
    assert_continuous(full, "Roadnet A current_pose -> RP-001 full")
    assert_continuous(local, "Roadnet A current_pose -> RP-001 local")
    assert dist(local[0], start_a) <= MAX_FIRST_POINT_DISTANCE_M, "RP-001 local does not start near current pose"
    assert local[0]["edge_id"] == "E_C-012_F", "RP-001 local lost current edge prefix"
    assert_no_reverse(local, "Roadnet A current_pose -> RP-001")

    rp008 = semantic_anchor(roadnet_a, "task_points", "RP-008")
    ok, note, full, _, _ = build_reference(roadnet_a, start_a, rp008, False, False)
    if ok:
        assert "reverse disabled; using forward detour" in note
        assert_no_reverse(full, "Roadnet A current_pose -> RP-008 reverse disabled")
        assert_continuous(full, "Roadnet A current_pose -> RP-008 reverse disabled")
    else:
        assert "reverse planning is disabled" in note

    ok, note, full, _, _ = build_reference(roadnet_a, start_a, rp008, True, True)
    assert ok, note
    assert "reverse local segment selected" in note
    assert any(int(wp.get("gear", 1)) == 2 for wp in full), "RP-008 reverse enabled did not produce reverse gear"
    assert_continuous(full, "Roadnet A current_pose -> RP-008 reverse enabled")

    start_b = current_anchor_from_pose(roadnet_b, ROADNET_B_CURRENT_POSE)
    rp003_b = semantic_anchor(roadnet_b, "task_points", "RP-003")
    ok, note, full, _, _ = build_reference(roadnet_b, start_b, rp003_b, False, False)
    assert ok, f"Roadnet B current_pose -> RP-003 regressed: {note}"
    assert_continuous(full, "Roadnet B current_pose -> RP-003")

    assert_config_defaults()
    print("offline_reverse_policy_smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
