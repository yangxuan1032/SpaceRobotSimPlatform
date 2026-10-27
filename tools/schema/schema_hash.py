#!/usr/bin/env python3
"""Compute SHA-256 hashes of every JSON schema under api/schema.

Writes api/schema/SCHEMA_HASHES.json (sorted, stable) so that recordings and
consumers can assert which schema revision produced a message.

Usage: python tools/schema/schema_hash.py [--check]

--check verifies the committed manifest matches the files on disk; without it
the manifest is rewritten.
"""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_DIR = ROOT / "api" / "schema"
MANIFEST = SCHEMA_DIR / "SCHEMA_HASHES.json"


def collect() -> dict[str, str]:
    hashes: dict[str, str] = {}
    for path in sorted(SCHEMA_DIR.rglob("*.json")):
        if path.name == MANIFEST.name:
            continue
        # Normalise line endings: a Windows checkout may materialise CRLF while
        # CI or Linux uses LF, and the hash must not depend on that.
        raw = path.read_bytes().replace(b"\r\n", b"\n")
        digest = hashlib.sha256(raw).hexdigest()
        hashes[path.relative_to(ROOT).as_posix()] = digest
    return hashes


def main(argv: list[str]) -> int:
    check = "--check" in argv[1:]
    hashes = collect()
    expected = json.dumps(hashes, indent=2, sort_keys=True) + "\n"
    if check:
        if not MANIFEST.exists():
            print("FAIL: manifest missing; run without --check")
            return 1
        current = MANIFEST.read_text(encoding="utf-8")
        if current != expected:
            print("FAIL: schema hashes are stale; run tools/schema/schema_hash.py")
            return 1
        print(f"PASS: {len(hashes)} schema hashes match")
        return 0
    MANIFEST.write_text(expected, encoding="utf-8")
    print(f"wrote {len(hashes)} hashes to {MANIFEST.relative_to(ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))