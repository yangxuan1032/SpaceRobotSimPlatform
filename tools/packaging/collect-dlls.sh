#!/usr/bin/env bash
# Collect the MinGW runtime DLLs required by an installed SRSP tree.
#
# Usage (MSYS2 UCRT64 shell):
#   ./tools/packaging/collect-dlls.sh <staging-dir> <destination-bin-dir>
#
# Walks every .exe/.dll under <staging-dir>, resolves imports with ldd and
# copies only libraries that live in the MSYS2 UCRT64 prefix. System DLLs
# (kernel32, ucrtbase, ...) are never copied.

set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <staging-dir> <destination-bin-dir>" >&2
  exit 2
fi

SRC="$1"
DEST="$2"
PREFIX="${MSYSTEM_PREFIX:-/ucrt64}"
PREFIX_WIN="${PREFIX#/}"

if [[ ! -d "${SRC}" ]]; then
  echo "error: staging directory not found: ${SRC}" >&2
  exit 1
fi

mkdir -p "${DEST}"

mapfile -t BINARIES < <(find "${SRC}" -type f \( -name '*.exe' -o -name '*.dll' \) -print)

if [[ ${#BINARIES[@]} -eq 0 ]]; then
  echo "error: no executables or DLLs under ${SRC}" >&2
  exit 1
fi

copied=0
while IFS= read -r binary; do
  while IFS= read -r dll; do
    [[ -f "${dll}" ]] || continue
    if cp -n "${dll}" "${DEST}/" 2>/dev/null; then
      copied=$((copied + 1))
      printf '  + %s\n' "$(basename "${dll}")"
    fi
  done < <(ldd "${binary}" 2>/dev/null | awk '{print $3}' | grep -F "/${PREFIX_WIN}/bin/" | sort -u)
done < <(printf '%s\n' "${BINARIES[@]}")

echo
echo "copied ${copied} runtime DLL(s) into ${DEST}"

missing=0
while IFS= read -r binary; do
  while IFS= read -r name; do
    [[ -n "${name}" ]] || continue
    if [[ ! -f "${DEST}/${name}" ]]; then
      printf '  ! %s imports %s (not found in %s)\n' "$(basename "${binary}")" "${name}" "${PREFIX}/bin"
      missing=$((missing + 1))
    fi
  done < <(objdump -p "${binary}" 2>/dev/null | awk '/DLL Name:/{print $3}' | grep -viE '^(kernel32|user32|gdi32|shell32|advapi32|ole32|oleaut32|comdlg32|comctl32|ws2_32|winmm|imm32|dwmapi|d3d11|d3d12|dxgi|opengl32|glu32|vulkan-1|ucrtbase|msvcrt|ntdll|api-ms-win-.*)\.dll$')
done < <(printf '%s\n' "${BINARIES[@]}")

if (( missing > 0 )); then
  echo "warning: ${missing} import(s) could not be resolved from the UCRT64 prefix" >&2
fi
