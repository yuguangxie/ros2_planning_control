#!/usr/bin/env python3
"""Offline checks for Ubuntu runtime follow-up fixes.

The checks mirror the ROS2 runtime issues without importing ROS2:
- semantic point null/"null" cleanup
- task/parking point linked_edge fallback
- planning trajectory republish configuration/static hooks
"""
from __future__ import annotations

import argparse
import heapq
import json
from pathlib import Path


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def clean_optional_string(value) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    if text.lower() in {"", "null", "none"}:
        return ""
    return text


def build_graph(topology):
    nodes = {node["id"] for node in topology["nodes"]}
    edges = {edge["id"]: edge for edge in topology["edges"]}
    return nodes, edges


def resolve_semantic_point(point, nodes, edges, prefer_edge_to_node: bool) -> str:
    linked_node = clean_optional_string(point.get("linked_node_id"))
    linked_edge = clean_optional_string(point.get("linked_edge_id"))
    assert linked_node != "null", "linked_node_id null leaked as literal string"
    if linked_node and linked_node in nodes:
        return linked_node
    if linked_edge and linked_edge in edges:
        edge = edges[linked_edge]
        return edge["to"] if prefer_edge_to_node else edge["from"]
    raise AssertionError(
        f"semantic point {point.get('id')} has no valid linked_node_id or linked_edge_id"
    )


def dijkstra(topology, start: str, goal: str):
    adj = {}
    for edge in topology["edges"]:
        if not edge.get("availability", {}).get("enabled", True):
            continue
        adj.setdefault(edge["from"], []).append(edge)
    pq = [(0.0, start, [])]
    best = {start: 0.0}
    while pq:
        cost, node, route = heapq.heappop(pq)
        if node == goal:
            return route
        if cost != best[node]:
            continue
        for edge in adj.get(node, []):
            next_cost = cost + float(edge.get("cost", edge.get("length_m", 1.0)))
            if next_cost < best.get(edge["to"], float("inf")):
                best[edge["to"]] = next_cost
                heapq.heappush(pq, (next_cost, edge["to"], route + [edge["id"]]))
    return []


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def static_republish_checks() -> None:
    planning_cpp = read_text(Path("src/low_speed_av_planning/src/planning_node.cpp"))
    planning_hpp = read_text(Path("src/low_speed_av_planning/include/low_speed_av_planning/planning_node.hpp"))
    planning_cfg = read_text(Path("src/low_speed_av_planning/config/planning_params.yaml"))
    bringup_cfg = read_text(Path("src/low_speed_av_bringup/config/planning_params.yaml"))

    required = [
        "planning.republish_last_trajectory",
        "planning.trajectory_republish_rate_hz",
        "republish_last_trajectory",
        "last_trajectory_msg_",
        "transient_local()",
        "planning.roadnet_status_publish_rate_hz",
    ]
    for token in required:
        assert token in planning_cpp or token in planning_hpp, f"missing planning source token: {token}"
    for token in [
        "republish_last_trajectory: true",
        "trajectory_republish_rate_hz: 10.0",
        "roadnet_status_publish_rate_hz: 1.0",
    ]:
        assert token in planning_cfg, f"missing planning config token: {token}"
        assert token in bringup_cfg, f"missing bringup config token: {token}"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "package", nargs="?", default="roadnet_ad_package_20260610T012525Z_2",
        help="formal Roadnet package (default: deterministic _2 fixture)",
    )
    args = parser.parse_args()

    root = Path(args.package)
    topology = load_json(root / "roadnet/topology.json")
    task_doc = load_json(root / "semantics/task_points.json")
    parking_doc = load_json(root / "semantics/parking_points.json")
    nodes, edges = build_graph(topology)

    task_targets = {}
    for point in task_doc["task_points"]:
        goal_node = resolve_semantic_point(point, nodes, edges, prefer_edge_to_node=True)
        start_node = resolve_semantic_point(point, nodes, edges, prefer_edge_to_node=False)
        task_targets[point["id"]] = (start_node, goal_node)
        assert goal_node in nodes
        assert start_node in nodes

    assert task_targets["RP-001"][1] == "N0008", task_targets["RP-001"]
    assert dijkstra(topology, "N0001", task_targets["RP-001"][1]), "N0001 -> RP-001 goal is not routable"
    route = dijkstra(topology, task_targets["RP-003"][0], task_targets["RP-001"][1])
    assert route, "RP-003 start -> RP-001 goal should be routable at node-level fallback"

    assert parking_doc.get("parking_points", []) == [], "current formal package unexpectedly has parking points"
    valid_parking = {
        "id": "P_FIXTURE_VALID",
        "type": "parking",
        "linked_node_id": None,
        "linked_edge_id": "E_C-001_F",
    }
    invalid_parking = {
        "id": "P_FIXTURE_INVALID",
        "type": "parking",
        "linked_node_id": None,
        "linked_edge_id": "BAD_EDGE",
    }
    assert resolve_semantic_point(valid_parking, nodes, edges, prefer_edge_to_node=True) == "N0002"
    try:
        resolve_semantic_point(invalid_parking, nodes, edges, prefer_edge_to_node=True)
    except AssertionError as exc:
        assert "no valid linked_node_id or linked_edge_id" in str(exc)
    else:
        raise AssertionError("invalid parking point was not rejected")

    null_literal_point = {
        "id": "T_NULL_LITERAL",
        "linked_node_id": "null",
        "linked_edge_id": "E_C-001_F",
    }
    assert resolve_semantic_point(null_literal_point, nodes, edges, prefer_edge_to_node=True) == "N0002"

    static_republish_checks()
    print(
        "Runtime follow-up smoke OK: "
        f"task_points={len(task_targets)}, RP-001_goal={task_targets['RP-001'][1]}, "
        f"RP-003_to_RP-001_edges={route}, parking_fixture=ok, republish_static=ok"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
