#!/usr/bin/env python3
"""Check that every Control YAML leaf is declared and consumed by production."""

from __future__ import annotations

import re
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[1]
PRODUCTION = ROOT / "src/low_speed_av_control/config/control_params.yaml"
BRINGUP = ROOT / "src/low_speed_av_bringup/config/control_params.yaml"
TEMPLATE = ROOT / "templates/sample_config/control_params.yaml"
NODE_SOURCE = ROOT / "src/low_speed_av_control/src/control_node.cpp"


def parameter_tree(path: Path) -> dict:
    document = yaml.safe_load(path.read_text(encoding="utf-8"))
    return document["low_speed_av_control"]["ros__parameters"]


def leaf_keys(value: object, prefix: str = "") -> set[str]:
    if not isinstance(value, dict):
        return {prefix}
    result: set[str] = set()
    for key, child in value.items():
        name = f"{prefix}.{key}" if prefix else str(key)
        result.update(leaf_keys(child, name))
    return result


def main() -> int:
    production = parameter_tree(PRODUCTION)
    if production != parameter_tree(BRINGUP):
        raise AssertionError("bringup Control config differs from canonical production config")
    if production != parameter_tree(TEMPLATE):
        raise AssertionError("template Control config differs from canonical production config")

    source = NODE_SOURCE.read_text(encoding="utf-8")
    declared = set(
        re.findall(r'declare_parameter[^\(]*\(\s*"([^"]+)"', source, re.DOTALL)
    )
    consumed = set(re.findall(r'get_parameter\(\s*"([^"]+)"', source, re.DOTALL))
    leaves = leaf_keys(production)
    undeclared = sorted(leaves - declared)
    unconsumed = sorted(leaves - consumed)
    declared_but_unused = sorted(declared - consumed)
    if undeclared or unconsumed or declared_but_unused:
        raise AssertionError(
            "Control parameter mapping mismatch: "
            f"undeclared={undeclared}, unconsumed={unconsumed}, "
            f"declared_but_unused={declared_but_unused}"
        )
    print(
        "Control config contract PASS: "
        f"yaml_leaves={len(leaves)} declared={len(declared)} consumed={len(consumed)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
