#!/usr/bin/env python3
"""Validate capability message examples against their JSON Schemas.

Layout:
    api/schema/capabilities/<capability>.schema.json   # has $defs.request / $defs.response
    api/schema/examples/capabilities/<capability>/{request,response}/
        valid-*.json    must validate
        invalid-*.json  must be rejected

Every direction must provide at least three valid and three invalid examples,
so the frozen message shape is pinned by tests (docs/02 section 5.3).
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

try:
    import jsonschema
except ImportError:  # pragma: no cover - environments without jsonschema skip
    print("SKIP: jsonschema is not installed")
    sys.exit(0)

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = ROOT / "api" / "schema" / "capabilities"
EXAMPLE_DIR = ROOT / "api" / "schema" / "examples" / "capabilities"

MIN_EXAMPLES = 3


def load(path: Path) -> dict:
    with path.open(encoding="utf-8") as handle:
        return json.load(handle)


def check_direction(capability: str, schema: dict, direction: str) -> list[str]:
    errors: list[str] = []
    sub = schema.get("$defs", {}).get(direction)
    if sub is None:
        return [f"{capability}: schema has no $defs.{direction}"]

    folder = EXAMPLE_DIR / capability / direction
    if not folder.is_dir():
        return [f"{capability}/{direction}: examples folder missing"]

    valid = sorted(folder.glob("valid-*.json"))
    invalid = sorted(folder.glob("invalid-*.json"))
    if len(valid) < MIN_EXAMPLES:
        errors.append(
            f"{capability}/{direction}: {len(valid)} valid examples, need {MIN_EXAMPLES}"
        )
    if len(invalid) < MIN_EXAMPLES:
        errors.append(
            f"{capability}/{direction}: {len(invalid)} invalid examples, need {MIN_EXAMPLES}"
        )

    for path in valid:
        instance = load(path)
        try:
            jsonschema.validate(instance, sub)
        except jsonschema.ValidationError as exc:
            errors.append(f"{path.name} should be valid: {exc.message}")
            continue
        print(f"  ok   {capability}/{direction}/{path.name}")

    for path in invalid:
        instance = load(path)
        try:
            jsonschema.validate(instance, sub)
        except jsonschema.ValidationError:
            print(f"  ok   {capability}/{direction}/{path.name} (rejected)")
            continue
        errors.append(f"{path.name} should be rejected but validated")

    return errors


def main() -> int:
    schemas = sorted(SCHEMA_DIR.glob("*.schema.json"))
    if not schemas:
        print(f"FAIL: no capability schemas under {SCHEMA_DIR}")
        return 1

    errors: list[str] = []
    for schema_path in schemas:
        capability = schema_path.name[: -len(".schema.json")]
        schema = load(schema_path)
        for direction in ("request", "response"):
            errors.extend(check_direction(capability, schema, direction))

    if errors:
        print("\nFAILED:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"\nPASS: {len(schemas)} capabilities, 3 valid and 3 invalid per direction")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
