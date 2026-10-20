#!/usr/bin/env bash
# Read-only availability report for the SRSP MSYS2 package set.
#
# Usage (from an "MSYS2 UCRT64" terminal):
#   ./tools/env/check-msys2-packages.sh
#
# Prints one line per package: installed / available / missing.
# Exit code is 1 when at least one package is neither installed nor available.

set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_FILE="${SCRIPT_DIR}/packages.txt"

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "error: run this from an MSYS2 UCRT64 shell (MSYSTEM=UCRT64)." >&2
  exit 1
fi

mapfile -t PACKAGES < <(grep -vE '^[[:space:]]*(#|$)' "${PACKAGE_FILE}")

installed=0
available=0
missing=0

for pkg in "${PACKAGES[@]}"; do
  if pacman -Q "${pkg}" >/dev/null 2>&1; then
    printf '  %-52s installed  %s\n' "${pkg}" "$(pacman -Q "${pkg}" | awk '{print $2}')"
    installed=$((installed + 1))
  elif pacman -Si "${pkg}" >/dev/null 2>&1; then
    printf '  %-52s available  %s\n' "${pkg}" "$(pacman -Si "${pkg}" | awk -F': ' '/^Version/{print $2; exit}')"
    available=$((available + 1))
  else
    printf '  %-52s MISSING\n' "${pkg}"
    missing=$((missing + 1))
  fi
done

printf '\ninstalled=%d available=%d missing=%d total=%d\n' \
  "${installed}" "${available}" "${missing}" "${#PACKAGES[@]}"

if (( missing > 0 )); then
  echo "Missing packages must be built from source (docs/05-msys2-dev-environment.md, section 5)."
  exit 1
fi
