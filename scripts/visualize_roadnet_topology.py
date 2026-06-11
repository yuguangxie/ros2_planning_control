#!/usr/bin/env python3
"""Generate an SVG topology visualization for a Low Speed Roadnet AD Package.

The script intentionally uses only Python's standard library so it can run in
Windows Codex, Ubuntu ROS2 shells, and CI environments without matplotlib.

Example:
  python scripts/visualize_roadnet_topology.py roadnet_ad_package_20260610T012525Z_1
  python scripts/visualize_roadnet_topology.py roadnet_ad_package_20260610T012525Z_2 --output reports/roadnet_2.svg
"""

from __future__ import annotations

import argparse
import html
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable


@dataclass
class NodeDraw:
    node_id: str
    x: float
    y: float
    inferred: bool = False


@dataclass
class EdgeDraw:
    edge_id: str
    from_node: str
    to_node: str
    direction: str
    cost: float
    length_m: float
    points: list[tuple[float, float]] = field(default_factory=list)


@dataclass
class SemanticPoint:
    point_id: str
    point_type: str
    x: float
    y: float
    yaw: float | None
    linked_edge_id: str
    linked_node_id: str
    name: str


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def as_float(value: Any, default: float = 0.0) -> float:
    try:
        out = float(value)
        return out if math.isfinite(out) else default
    except (TypeError, ValueError):
        return default


def clean_optional_string(value: Any) -> str:
    if value is None:
        return ""
    text = str(value).strip()
    return "" if text.lower() in {"", "null", "none"} else text


def extract_pose(item: dict[str, Any]) -> tuple[float | None, float | None, float | None]:
    pose = item.get("pose") if isinstance(item.get("pose"), dict) else {}
    props = item.get("properties") if isinstance(item.get("properties"), dict) else {}
    x = pose.get("x", props.get("x"))
    y = pose.get("y", props.get("y"))
    yaw = pose.get("yaw", props.get("yaw", props.get("docking_yaw")))
    if x is None or y is None:
        return None, None, None
    return as_float(x), as_float(y), None if yaw is None else as_float(yaw)


def edge_reference_points(edge: dict[str, Any]) -> list[tuple[float, float]]:
    points: list[tuple[float, float]] = []
    for point in edge.get("reference_points", []) or []:
        if not isinstance(point, dict) or "x" not in point or "y" not in point:
            continue
        points.append((as_float(point["x"]), as_float(point["y"])))
    return points


def parse_topology(package_root: Path) -> tuple[list[NodeDraw], list[EdgeDraw], str]:
    topology_path = package_root / "roadnet" / "topology.json"
    topology = load_json(topology_path)
    if not topology:
        raise FileNotFoundError(f"missing or empty topology file: {topology_path}")

    raw_edges = topology.get("edges", [])
    if not isinstance(raw_edges, list):
        raise ValueError(f"topology edges must be a list: {topology_path}")

    nodes: dict[str, NodeDraw] = {}
    raw_nodes = topology.get("nodes", [])
    if isinstance(raw_nodes, list):
        for item in raw_nodes:
            if not isinstance(item, dict):
                continue
            node_id = clean_optional_string(item.get("id") or item.get("node_id"))
            x = item.get("x")
            y = item.get("y")
            pose = item.get("pose") if isinstance(item.get("pose"), dict) else {}
            if x is None:
                x = pose.get("x")
            if y is None:
                y = pose.get("y")
            if node_id and x is not None and y is not None:
                nodes[node_id] = NodeDraw(node_id=node_id, x=as_float(x), y=as_float(y), inferred=False)

    edge_draws: list[EdgeDraw] = []
    inferred_positions: dict[str, list[tuple[float, float]]] = {}
    for edge in raw_edges:
        if not isinstance(edge, dict):
            continue
        edge_id = clean_optional_string(edge.get("id"))
        from_node = clean_optional_string(edge.get("from") or edge.get("from_node_id"))
        to_node = clean_optional_string(edge.get("to") or edge.get("to_node_id"))
        if not edge_id or not from_node or not to_node:
            continue
        points = edge_reference_points(edge)
        edge_draws.append(
            EdgeDraw(
                edge_id=edge_id,
                from_node=from_node,
                to_node=to_node,
                direction=clean_optional_string(edge.get("direction")) or "forward",
                cost=as_float(edge.get("cost"), as_float(edge.get("length_m"))),
                length_m=as_float(edge.get("length_m"), as_float(edge.get("cost"))),
                points=points,
            )
        )
        if points:
            inferred_positions.setdefault(from_node, []).append(points[0])
            inferred_positions.setdefault(to_node, []).append(points[-1])

    for node_id, samples in inferred_positions.items():
        if node_id in nodes or not samples:
            continue
        x = sum(p[0] for p in samples) / len(samples)
        y = sum(p[1] for p in samples) / len(samples)
        nodes[node_id] = NodeDraw(node_id=node_id, x=x, y=y, inferred=True)

    return sorted(nodes.values(), key=lambda n: n.node_id), edge_draws, str(topology.get("coordinate_frame", "map"))


def semantic_linked_edge(item: dict[str, Any]) -> str:
    for key in ("linked_edge_id", "entry_edge_id", "exit_edge_id"):
        value = clean_optional_string(item.get(key))
        if value:
            return value
    approach = item.get("approach")
    if isinstance(approach, dict):
        return clean_optional_string(approach.get("edge_id"))
    return ""


def parse_semantic_file(path: Path, array_key: str, point_type: str) -> list[SemanticPoint]:
    data = load_json(path)
    items = data.get(array_key, [])
    if not isinstance(items, list):
        return []
    out: list[SemanticPoint] = []
    for item in items:
        if not isinstance(item, dict):
            continue
        x, y, yaw = extract_pose(item)
        if x is None or y is None:
            continue
        props = item.get("properties") if isinstance(item.get("properties"), dict) else {}
        out.append(
            SemanticPoint(
                point_id=clean_optional_string(item.get("id") or item.get("point_id")),
                point_type=point_type,
                x=x,
                y=y,
                yaw=yaw,
                linked_edge_id=semantic_linked_edge(item),
                linked_node_id=clean_optional_string(item.get("linked_node_id")),
                name=clean_optional_string(props.get("name")),
            )
        )
    return out


def parse_semantics(package_root: Path) -> list[SemanticPoint]:
    sem = package_root / "semantics"
    points: list[SemanticPoint] = []
    points.extend(parse_semantic_file(sem / "task_points.json", "task_points", "task"))
    points.extend(parse_semantic_file(sem / "parking_points.json", "parking_points", "parking"))
    points.extend(parse_semantic_file(sem / "charging_points.json", "charging_points", "charging"))
    points.extend(parse_semantic_file(sem / "route_points.json", "route_points", "route"))
    return points


def bounds_for(
    nodes: Iterable[NodeDraw],
    edges: Iterable[EdgeDraw],
    semantics: Iterable[SemanticPoint],
) -> tuple[float, float, float, float]:
    xs: list[float] = []
    ys: list[float] = []
    for node in nodes:
        xs.append(node.x)
        ys.append(node.y)
    for edge in edges:
        for x, y in edge.points:
            xs.append(x)
            ys.append(y)
    for point in semantics:
        xs.append(point.x)
        ys.append(point.y)
    if not xs or not ys:
        return -1.0, -1.0, 1.0, 1.0
    return min(xs), min(ys), max(xs), max(ys)


def make_transform(
    bounds: tuple[float, float, float, float],
    width: int,
    height: int,
    margin: int,
):
    min_x, min_y, max_x, max_y = bounds
    span_x = max(max_x - min_x, 1e-6)
    span_y = max(max_y - min_y, 1e-6)
    scale = min((width - 2 * margin) / span_x, (height - 2 * margin) / span_y)
    used_w = span_x * scale
    used_h = span_y * scale
    offset_x = margin + (width - 2 * margin - used_w) / 2
    offset_y = margin + (height - 2 * margin - used_h) / 2

    def project(x: float, y: float) -> tuple[float, float]:
        sx = offset_x + (x - min_x) * scale
        sy = height - (offset_y + (y - min_y) * scale)
        return sx, sy

    return project


def path_d(points: list[tuple[float, float]], project) -> str:
    if not points:
        return ""
    projected = [project(x, y) for x, y in points]
    first = projected[0]
    commands = [f"M {first[0]:.2f} {first[1]:.2f}"]
    for x, y in projected[1:]:
        commands.append(f"L {x:.2f} {y:.2f}")
    return " ".join(commands)


def arrow_marker(marker_id: str, color: str) -> str:
    return (
        f'<marker id="{marker_id}" markerWidth="9" markerHeight="9" '
        'refX="8" refY="3" orient="auto" markerUnits="strokeWidth">'
        f'<path d="M0,0 L0,6 L8,3 z" fill="{color}" /></marker>'
    )


def render_svg(
    package_root: Path,
    nodes: list[NodeDraw],
    edges: list[EdgeDraw],
    semantics: list[SemanticPoint],
    frame_id: str,
    output: Path,
    width: int,
    height: int,
) -> None:
    margin = 72
    project = make_transform(bounds_for(nodes, edges, semantics), width, height, margin)
    node_by_id = {node.node_id: node for node in nodes}
    semantic_colors = {
        "task": "#e11d48",
        "parking": "#2563eb",
        "charging": "#16a34a",
        "route": "#9333ea",
    }

    parts: list[str] = []
    parts.append('<?xml version="1.0" encoding="UTF-8"?>')
    parts.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">'
    )
    parts.append("<defs>")
    parts.append(arrow_marker("arrow-forward", "#374151"))
    parts.append(arrow_marker("arrow-reverse", "#b45309"))
    parts.append("</defs>")
    parts.append('<rect width="100%" height="100%" fill="#f8fafc"/>')
    parts.append(
        f'<text x="24" y="32" font-size="18" font-family="Arial" fill="#111827">'
        f'Roadnet topology: {html.escape(package_root.name)} / frame={html.escape(frame_id)}</text>'
    )
    parts.append(
        f'<text x="24" y="56" font-size="12" font-family="Arial" fill="#475569">'
        f'nodes={len(nodes)} edges={len(edges)} semantic_points={len(semantics)}</text>'
    )

    parts.append('<g id="edges" fill="none">')
    for edge in edges:
        pts = edge.points
        if len(pts) < 2:
            from_node = node_by_id.get(edge.from_node)
            to_node = node_by_id.get(edge.to_node)
            if from_node and to_node:
                pts = [(from_node.x, from_node.y), (to_node.x, to_node.y)]
        if len(pts) < 2:
            continue
        color = "#b45309" if edge.direction == "reverse" else "#374151"
        arrow = "arrow-reverse" if edge.direction == "reverse" else "arrow-forward"
        parts.append(
            f'<path d="{path_d(pts, project)}" stroke="{color}" stroke-width="1.3" '
            f'opacity="0.68" marker-end="url(#{arrow})">'
            f'<title>{html.escape(edge.edge_id)}: {html.escape(edge.from_node)} -> '
            f'{html.escape(edge.to_node)}, direction={html.escape(edge.direction)}, '
            f'length={edge.length_m:.3f}m</title></path>'
        )
        mid = pts[len(pts) // 2]
        mx, my = project(*mid)
        parts.append(
            f'<text x="{mx:.2f}" y="{my:.2f}" font-size="8" font-family="Arial" '
            f'fill="{color}" opacity="0.8">{html.escape(edge.edge_id)}</text>'
        )
    parts.append("</g>")

    parts.append('<g id="nodes">')
    for node in nodes:
        x, y = project(node.x, node.y)
        fill = "#ffffff" if not node.inferred else "#fef3c7"
        parts.append(
            f'<circle cx="{x:.2f}" cy="{y:.2f}" r="4.5" fill="{fill}" '
            f'stroke="#0f172a" stroke-width="1.1"><title>{html.escape(node.node_id)}'
            f' x={node.x:.3f} y={node.y:.3f} inferred={node.inferred}</title></circle>'
        )
        parts.append(
            f'<text x="{x + 6:.2f}" y="{y - 6:.2f}" font-size="9" font-family="Arial" '
            f'fill="#0f172a">{html.escape(node.node_id)}</text>'
        )
    parts.append("</g>")

    parts.append('<g id="semantic-points">')
    for point in semantics:
        x, y = project(point.x, point.y)
        color = semantic_colors.get(point.point_type, "#64748b")
        shape = "rect" if point.point_type == "parking" else "circle"
        label = f"{point.point_type}:{point.point_id}"
        title = (
            f"{label} x={point.x:.3f} y={point.y:.3f} yaw={point.yaw} "
            f"edge={point.linked_edge_id} node={point.linked_node_id}"
        )
        if shape == "rect":
            parts.append(
                f'<rect x="{x - 5:.2f}" y="{y - 5:.2f}" width="10" height="10" '
                f'fill="{color}" stroke="#ffffff" stroke-width="1.2"><title>'
                f'{html.escape(title)}</title></rect>'
            )
        else:
            radius = "6" if point.point_type in {"task", "charging"} else "5"
            parts.append(
                f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{radius}" fill="{color}" '
                f'stroke="#ffffff" stroke-width="1.2"><title>{html.escape(title)}</title></circle>'
            )
        if point.yaw is not None:
            x2 = x + 18.0 * math.cos(point.yaw)
            y2 = y - 18.0 * math.sin(point.yaw)
            parts.append(
                f'<line x1="{x:.2f}" y1="{y:.2f}" x2="{x2:.2f}" y2="{y2:.2f}" '
                f'stroke="{color}" stroke-width="2" marker-end="url(#arrow-forward)" opacity="0.9"/>'
            )
        parts.append(
            f'<text x="{x + 8:.2f}" y="{y + 4:.2f}" font-size="11" font-family="Arial" '
            f'font-weight="bold" fill="{color}">{html.escape(point.point_id)}</text>'
        )
    parts.append("</g>")

    legend_y = height - 96
    legend = [
        ("#374151", "forward edge"),
        ("#b45309", "reverse edge"),
        ("#e11d48", "task point"),
        ("#2563eb", "parking point"),
        ("#16a34a", "charging point"),
        ("#9333ea", "route point"),
    ]
    parts.append('<g id="legend" font-family="Arial" font-size="12">')
    for i, (color, label) in enumerate(legend):
        lx = 24 + (i % 3) * 180
        ly = legend_y + (i // 3) * 24
        parts.append(f'<rect x="{lx}" y="{ly - 10}" width="14" height="14" fill="{color}"/>')
        parts.append(f'<text x="{lx + 20}" y="{ly + 1}" fill="#111827">{html.escape(label)}</text>')
    parts.append("</g>")

    parts.append("</svg>")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(parts), encoding="utf-8")


def write_summary(
    output: Path,
    package_root: Path,
    nodes: list[NodeDraw],
    edges: list[EdgeDraw],
    semantics: list[SemanticPoint],
    frame_id: str,
) -> None:
    by_type: dict[str, int] = {}
    for point in semantics:
        by_type[point.point_type] = by_type.get(point.point_type, 0) + 1
    summary = {
        "package": str(package_root),
        "frame_id": frame_id,
        "nodes": len(nodes),
        "inferred_nodes": sum(1 for node in nodes if node.inferred),
        "edges": len(edges),
        "semantic_points": by_type,
        "edge_directions": {
            direction: sum(1 for edge in edges if edge.direction == direction)
            for direction in sorted({edge.direction for edge in edges})
        },
    }
    output.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "package",
        nargs="?",
        default="roadnet_ad_package_20260610T012525Z_1",
        help="Roadnet AD Package directory. Default: roadnet_ad_package_20260610T012525Z_1",
    )
    parser.add_argument(
        "--output",
        "-o",
        default=None,
        help="Output SVG path. Default: reports/<package>_topology.svg",
    )
    parser.add_argument(
        "--summary",
        default=None,
        help="Optional JSON summary path. Default: reports/<package>_topology_summary.json",
    )
    parser.add_argument("--width", type=int, default=1600, help="SVG width in px")
    parser.add_argument("--height", type=int, default=1100, help="SVG height in px")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    package_root = Path(args.package).resolve()
    if not package_root.exists():
        raise SystemExit(f"roadnet package does not exist: {package_root}")
    default_stem = package_root.name
    svg_path = Path(args.output) if args.output else Path("reports") / f"{default_stem}_topology.svg"
    summary_path = Path(args.summary) if args.summary else Path("reports") / f"{default_stem}_topology_summary.json"

    nodes, edges, frame_id = parse_topology(package_root)
    semantics = parse_semantics(package_root)
    render_svg(package_root, nodes, edges, semantics, frame_id, svg_path, args.width, args.height)
    write_summary(summary_path, package_root, nodes, edges, semantics, frame_id)

    print(f"Generated SVG: {svg_path}")
    print(f"Generated summary: {summary_path}")
    print(
        f"Roadnet stats: nodes={len(nodes)} edges={len(edges)} "
        f"semantic_points={len(semantics)} frame={frame_id}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
