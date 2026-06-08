#!/usr/bin/env python3
"""Validate Low Speed Roadnet AD Package v1.1 sample without ROS2."""
from __future__ import annotations
import argparse, hashlib, json, math, sys
from pathlib import Path

try:
    import yaml
except Exception:  # pragma: no cover
    yaml = None

REQUIRED = [
    "project_manifest.json",
    "checksums.sha256",
    "map/map_metadata.yaml",
    "roadnet/roadnet.json",
    "roadnet/topology.json",
    "roadnet/route_graph.yaml",
    "trajectory/waypoints.yaml",
    "trajectory/waypoints.csv",
    "trajectory/waypoint_index.json",
    "semantics/areas.json",
    "semantics/route_points.json",
    "semantics/task_points.json",
    "semantics/parking_points.json",
    "semantics/charging_points.json",
    "validation/validation_report.json",
]

WP_FIELDS = {"global_index", "waypoint_id", "edge_id", "path_id", "s_m", "x", "y", "yaw", "kappa", "v_mps"}

def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))

def load_yaml(path: Path):
    if yaml is None:
        raise RuntimeError("PyYAML is required to parse waypoints.yaml in this offline validator")
    return yaml.safe_load(path.read_text(encoding="utf-8"))

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    h.update(path.read_bytes())
    return h.hexdigest()

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("package", nargs="?", default="templates/sample_ad_package")
    args = ap.parse_args()
    root = Path(args.package)
    errors = []
    warnings = []

    for rel in REQUIRED:
        if not (root / rel).exists():
            errors.append(f"missing required file: {rel}")

    if errors:
        print("\n".join(errors))
        return 1

    manifest = load_json(root / "project_manifest.json")
    if manifest.get("schema") != "low_speed_roadnet_ad_package":
        errors.append("manifest schema mismatch")
    if not str(manifest.get("schema_version", "")).startswith("1.1"):
        errors.append("unsupported schema_version")
    validation = manifest.get("validation", {})
    if validation.get("status") == "failed" or int(validation.get("blocking_errors", 0)) > 0:
        errors.append("manifest validation failed")

    report = load_json(root / manifest.get("files", {}).get("validation_report", "validation/validation_report.json"))
    if report.get("status") == "failed" or int(report.get("summary", {}).get("blocking_errors", 0)) > 0:
        errors.append("validation report failed")

    topology = load_json(root / manifest.get("files", {}).get("topology", "roadnet/topology.json"))
    nodes = {n["id"]: n for n in topology.get("nodes", [])}
    edges = {e["id"]: e for e in topology.get("edges", [])}
    for e in edges.values():
        if e.get("from") not in nodes:
            errors.append(f"edge {e.get('id')} unknown from node")
        if e.get("to") not in nodes:
            errors.append(f"edge {e.get('id')} unknown to node")

    waypoints_doc = load_yaml(root / manifest.get("files", {}).get("waypoints_yaml", "trajectory/waypoints.yaml"))
    waypoints = waypoints_doc.get("waypoints", [])
    if not waypoints:
        errors.append("waypoints.yaml has no waypoints")
    for i, wp in enumerate(waypoints):
        missing = WP_FIELDS - set(wp.keys())
        if missing:
            errors.append(f"waypoint {i} missing fields {sorted(missing)}")
        for key in ["x", "y", "yaw", "kappa", "v_mps", "s_m"]:
            if key in wp and not math.isfinite(float(wp[key])):
                errors.append(f"waypoint {i} {key} is not finite")

    idx = load_json(root / manifest.get("files", {}).get("waypoint_index", "trajectory/waypoint_index.json"))
    for edge_id, rng in idx.get("edges", {}).items():
        if edge_id not in edges:
            errors.append(f"waypoint_index references unknown edge {edge_id}")
        start = int(rng.get("start_index", -1))
        end_excl = int(rng.get("end_index_exclusive", int(rng.get("end_index", -1)) + 1))
        if start < 0 or end_excl < start or end_excl > len(waypoints):
            errors.append(f"invalid waypoint range for {edge_id}: {start}:{end_excl}")

    checksums = root / "checksums.sha256"
    if checksums.exists():
        for line in checksums.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            digest, rel = line.split(None, 1)
            rel = rel.strip()
            if rel == "checksums.sha256":
                continue
            p = root / rel
            if not p.exists():
                errors.append(f"checksum references missing file {rel}")
            elif sha256(p) != digest:
                errors.append(f"checksum mismatch for {rel}")
    else:
        warnings.append("checksums.sha256 missing")

    if warnings:
        print("WARNINGS:")
        print("\n".join(warnings))
    if errors:
        print("ERRORS:")
        print("\n".join(errors))
        return 1
    print(f"AD Package OK: {root} ({len(nodes)} nodes, {len(edges)} edges, {len(waypoints)} waypoints)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
