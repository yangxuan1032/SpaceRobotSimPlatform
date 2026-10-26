#!/usr/bin/env python3
"""Repository hygiene checks used by CI and by `ctest -R docs_check`.

Checks:
  1. every tracked JSON file parses;
  2. every YAML file parses (skipped when PyYAML is unavailable);
  3. every relative Markdown link resolves to an existing file.

Exit code 0 when everything passes, 1 otherwise. Deliberately dependency-light:
JSON uses the standard library, YAML is optional.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SKIP_DIRS = {".git", "Temp", "build", "node_modules", "__pycache__", ".vs"}
JSON_TARGETS = ["api", "tests", "tools", "docs", "deepseek", "ai-models.json",
                "CMakePresets.json", "vcpkg.json"]
LINK_RE = re.compile(r"\]\(([^)\s]+)\)")


def iter_files(pattern: str):
    for target in JSON_TARGETS:
        path = ROOT / target
        if path.is_file():
            yield path
        elif path.is_dir():
            for candidate in path.rglob(pattern):
                if not any(part in SKIP_DIRS for part in candidate.parts):
                    yield candidate


def check_json() -> list[str]:
    errors = []
    for path in sorted(set(iter_files("*.json"))):
        try:
            with path.open(encoding="utf-8") as handle:
                json.load(handle)
        except json.JSONDecodeError as exc:
            errors.append(f"JSON parse error in {path.relative_to(ROOT)}: {exc}")
    return errors


def check_yaml() -> tuple[list[str], bool]:
    try:
        import yaml  # type: ignore
    except ImportError:
        return [], False

    errors = []
    for suffix in ("*.yml", "*.yaml"):
        for path in sorted(set(iter_files(suffix))):
            try:
                with path.open(encoding="utf-8") as handle:
                    yaml.safe_load(handle)
            except yaml.YAMLError as exc:  # type: ignore[attr-defined]
                errors.append(f"YAML parse error in {path.relative_to(ROOT)}: {exc}")
    return errors, True


def check_markdown_links() -> list[str]:
    errors = []
    for path in sorted(set(iter_files("*.md"))):
        text = path.read_text(encoding="utf-8", errors="replace")
        for match in LINK_RE.finditer(text):
            target = match.group(1)
            if target.startswith(("http://", "https://", "mailto:", "#")):
                continue
            # strip anchors and query strings
            clean = target.split("#", 1)[0].split("?", 1)[0]
            if not clean:
                continue
            resolved = (path.parent / clean).resolve()
            if not resolved.exists():
                errors.append(f"broken link in {path.relative_to(ROOT)}: {target}")
    return errors


def main() -> int:
    json_errors = check_json()
    yaml_errors, yaml_enabled = check_yaml()
    link_errors = check_markdown_links()

    print(f"json files checked: ok={not json_errors}")
    print(f"yaml files checked: {'ok=' + str(not yaml_errors) if yaml_enabled else 'skipped (PyYAML missing)'}")
    print(f"markdown links: ok={not link_errors}")

    errors = json_errors + yaml_errors + link_errors
    if errors:
        print("\nFAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print("\nPASS: docs and data files are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
