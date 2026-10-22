"""Validate plugin manifest examples against plugin-manifest.schema.json.

Naming convention: valid-*.json must validate, invalid-*.json must be rejected.
Exit code 0 when every expectation matches, 1 otherwise.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import jsonschema

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "api" / "schema" / "plugin-manifest.schema.json"
EXAMPLES = ROOT / "api" / "schema" / "examples"


def main() -> int:
    schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
    validator = jsonschema.validators.validator_for(schema)(schema)

    failures = 0
    for path in sorted(EXAMPLES.glob("*.json")):
        expect_valid = path.name.startswith("valid-")
        doc = json.loads(path.read_text(encoding="utf-8"))
        errors = sorted(validator.iter_errors(doc), key=lambda e: e.json_path)
        is_valid = not errors
        status = "OK  " if is_valid == expect_valid else "FAIL"
        want = "valid  " if expect_valid else "invalid"
        print(f"{status} {path.name:<48} expect={want} got={'valid' if is_valid else 'invalid'}")
        if is_valid != expect_valid:
            failures += 1
            for err in errors[:5]:
                print(f"      {err.json_path}: {err.message}")

    print(f"\n{failures} expectation failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
