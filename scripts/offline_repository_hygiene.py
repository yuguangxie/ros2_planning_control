#!/usr/bin/env python3
"""Check repository text, JSON, Markdown links, and test hardware isolation."""

from __future__ import annotations

import ast
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


TEXT_SUFFIXES = {
    ".cmake", ".cpp", ".csv", ".hpp", ".json", ".md", ".msg", ".py",
    ".srv", ".txt", ".xml", ".yaml", ".yml",
}
IGNORED_PARTS = {".git", ".pytest_cache", "__pycache__", "build", "install", "log"}
MARKDOWN_LINK = re.compile(r"!?\[[^\]]*\]\(([^)]+)\)")
REAL_DEVICE_IP = re.compile(r"\b192\.168\.\d{1,3}\.\d{1,3}\b")


def repository_files(root: Path) -> list[Path]:
    return sorted(
        path
        for path in root.rglob("*")
        if path.is_file() and not any(part in IGNORED_PARTS for part in path.parts)
    )


def local_link_target(raw: str) -> str | None:
    target = raw.strip()
    if target.startswith("<") and ">" in target:
        target = target[1:target.index(">")]
    else:
        target = target.split(maxsplit=1)[0]
    if not target or target.startswith(("#", "http://", "https://", "mailto:")):
        return None
    return target.split("#", 1)[0]


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    files = repository_files(root)
    errors: list[str] = []

    decoded: dict[Path, str] = {}
    for path in files:
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in {"CMakeLists.txt"}:
            continue
        try:
            decoded[path] = path.read_text(encoding="utf-8")
        except UnicodeDecodeError as exc:
            errors.append(f"UTF8 {path.relative_to(root)}: {exc}")

    for path, text in decoded.items():
        if path.suffix.lower() == ".json":
            try:
                json.loads(text)
            except json.JSONDecodeError as exc:
                errors.append(f"JSON {path.relative_to(root)}: {exc}")

        if path.suffix.lower() == ".py":
            try:
                ast.parse(text, filename=str(path))
            except SyntaxError as exc:
                errors.append(f"PYTHON_SYNTAX {path.relative_to(root)}: {exc}")

        if path.suffix.lower() == ".xml":
            try:
                ET.fromstring(text)
            except ET.ParseError as exc:
                errors.append(f"XML {path.relative_to(root)}: {exc}")

        if path.suffix.lower() == ".md":
            for match in MARKDOWN_LINK.finditer(text):
                target = local_link_target(match.group(1))
                if target is None:
                    continue
                candidate = (root / target.lstrip("/")) if target.startswith("/") else (path.parent / target)
                if not candidate.exists():
                    errors.append(
                        f"MARKDOWN_LINK {path.relative_to(root)} -> {target}"
                    )

    isolation_roots = [root / ".github", root / "scripts"]
    isolation_roots.extend(path for path in (root / "src").glob("*/test") if path.exists())
    isolation_roots.extend(path for path in (root / "src/yunle_chassis").glob("*/test") if path.exists())
    for base in isolation_roots:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if path.is_file() and path.suffix.lower() in TEXT_SUFFIXES:
                text = path.read_text(encoding="utf-8")
                if REAL_DEVICE_IP.search(text):
                    errors.append(f"REAL_DEVICE_IP {path.relative_to(root)}")

    git_result = subprocess.run(
        ["git", "-c", f"safe.directory={root.as_posix()}", "ls-files"],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    if git_result.returncode != 0:
        stderr = git_result.stderr.strip() or "<empty stderr>"
        errors.append(f"GIT_LS_FILES exit={git_result.returncode}: {stderr}")
        tracked: list[str] = []
    else:
        tracked = git_result.stdout.splitlines()
    for name in tracked:
        parts = Path(name).parts
        if any(part in {"build", "install", "log", "__pycache__", ".pytest_cache"} for part in parts):
            errors.append(f"GENERATED_TRACKED {name}")

    if errors:
        print("Repository hygiene FAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1

    json_count = sum(path.suffix.lower() == ".json" for path in decoded)
    markdown_count = sum(path.suffix.lower() == ".md" for path in decoded)
    print(
        "Repository hygiene PASS: "
        f"utf8_files={len(decoded)} json_files={json_count} markdown_files={markdown_count}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
