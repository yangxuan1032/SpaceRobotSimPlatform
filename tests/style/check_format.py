#!/usr/bin/env python3
"""Verify that every C/C++ source matches the repository .clang-format.

Runs `clang-format --dry-run --Werror` over src/, apps/, tests/ and examples/.
The formatter is located from CLANG_FORMAT, PATH, or the MSYS2 UCRT64 default.
When no formatter is available the check prints a SKIP message and exits 0, so
toolchains without clang-format (for example MSVC-only jobs) still pass.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRS = ["src", "apps", "tests", "examples"]
SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}
SKIP_DIRS = {"Temp", "build", "node_modules", ".git"}
FALLBACKS = [
    r"C:/msys64/ucrt64/bin/clang-format.exe",
    "/usr/bin/clang-format",
    "/usr/local/bin/clang-format",
]


def find_formatter() -> str | None:
    explicit = os.environ.get("CLANG_FORMAT")
    if explicit and Path(explicit).exists():
        return explicit
    found = shutil.which("clang-format")
    if found:
        return found
    for candidate in FALLBACKS:
        if Path(candidate).exists():
            return candidate
    return None


def sources() -> list[Path]:
    files: list[Path] = []
    for directory in DIRS:
        base = ROOT / directory
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if any(part in SKIP_DIRS for part in path.parts):
                continue
            if path.is_file() and path.suffix in SUFFIXES:
                files.append(path)
    return sorted(files)


def main() -> int:
    formatter = find_formatter()
    if formatter is None:
        print("SKIP: clang-format not found (set CLANG_FORMAT to enable)")
        return 0

    files = sources()
    if not files:
        print("FAIL: no C/C++ sources found")
        return 1

    failures: list[str] = []
    for path in files:
        result = subprocess.run(
            [formatter, "--dry-run", "--Werror", str(path)],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            failures.append(path.relative_to(ROOT).as_posix())

    print(f"checked {len(files)} files with {formatter}")
    if failures:
        print("\nFAILED (run clang-format -i on these):")
        for name in failures:
            print(f"  - {name}")
        return 1

    print("PASS: all sources match .clang-format")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
