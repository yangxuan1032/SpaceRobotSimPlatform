#!/usr/bin/env bash
# Bootstrap the SRSP toolchain inside MSYS2 UCRT64.
#
# Usage (from an "MSYS2 UCRT64" terminal):
#   ./tools/env/bootstrap-msys2.sh
#
# The script is idempotent: installed packages are skipped, unavailable ones
# are reported at the end instead of aborting the whole run.

set -uo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PACKAGE_FILE="${SCRIPT_DIR}/packages.txt"

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
  echo "error: MSYSTEM=${MSYSTEM:-<unset>}; run this from an MSYS2 UCRT64 shell." >&2
  echo "       Start menu -> 'MSYS2 UCRT64'." >&2
  exit 1
fi

if [[ ! -f "${PACKAGE_FILE}" ]]; then
  echo "error: package list not found: ${PACKAGE_FILE}" >&2
  exit 1
fi

say() { printf '\n== %s ==\n' "$*"; }

say "Updating package databases (pacman -Syu)"
if ! pacman -Syu --noconfirm; then
  echo "warning: 'pacman -Syu' failed. If it asked to close this window, reopen the" >&2
  echo "         MSYS2 UCRT64 terminal and run this script again." >&2
fi

mapfile -t PACKAGES < <(grep -vE '^[[:space:]]*(#|$)' "${PACKAGE_FILE}")

MISSING=()
FAILED=()
INSTALLED=0

install_one() {
  local pkg="$1"

  if pacman -Q "${pkg}" >/dev/null 2>&1; then
    printf '  [present]  %s\n' "${pkg}"
    return 0
  fi

  if ! pacman -Si "${pkg}" >/dev/null 2>&1; then
    printf '  [missing]  %s (not in configured repositories)\n' "${pkg}"
    MISSING+=("${pkg}")
    return 0
  fi

  if pacman -S --needed --noconfirm "${pkg}"; then
    printf '  [ok]       %s\n' "${pkg}"
    INSTALLED=$((INSTALLED + 1))
  else
    printf '  [FAILED]   %s\n' "${pkg}"
    FAILED+=("${pkg}")
  fi
}

say "Installing ${#PACKAGES[@]} packages"
for pkg in "${PACKAGES[@]}"; do
  install_one "${pkg}"
done

say "Summary"
printf '  newly installed: %d\n' "${INSTALLED}"

if (( ${#MISSING[@]} > 0 )); then
  printf '  not in repositories (%d):\n' "${#MISSING[@]}"
  printf '    %s\n' "${MISSING[@]}"
  echo "  -> build these from source under third_party/ (docs/05, section 5)."
fi

if (( ${#FAILED[@]} > 0 )); then
  printf '  failed (%d):\n' "${#FAILED[@]}"
  printf '    %s\n' "${FAILED[@]}"
  exit 1
fi

say "Toolchain versions"
for tool in gcc g++ cmake ninja; do
  if command -v "${tool}" >/dev/null 2>&1; then
    printf '  %-6s %s\n' "${tool}" "$("${tool}" --version | head -n 1)"
  else
    printf '  %-6s <not found>\n' "${tool}"
  fi
done

echo
echo "Done. Next:"
echo "  ./tools/env/launch-vscode.ps1        # from PowerShell"
echo "  cmake --preset win-ucrt64-debug"
