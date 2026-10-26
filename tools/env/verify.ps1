# Environment self-check for the SRSP toolchain (read-only, no installs).
#
# Usage: pwsh -File tools/env/verify.ps1
# Exit code 0 when every required check passes; 1 otherwise.
$ErrorActionPreference = "Continue"
# MSYS2 tools need their DLLs on PATH to start.
$env:Path = "C:\msys64\ucrt64\bin;" + $env:Path

function Get-Version($exe, $argList) {
  # cmd /c keeps the child's stdout/stderr and exit code independent of the
  # PowerShell pipeline, which made cmake/ctest/python look missing (P30).
  try {
    $line = (cmd /c "`"$exe`" $($argList -join ' ') 2>&1" | Select-Object -First 1)
    if ([string]::IsNullOrWhiteSpace($line)) { return $null }
    # cmd /c prints its own error when the executable is missing; treat it as absent.
    if ($line -match 'not recognized|not found|不是内部或外部命令|Cannot find') { return $null }
    return $line.Trim()
  } catch { return $null }
}

$failures = @()
$rows = @()

function Add-Check($name, $exe, $argList, $required, $hint) {
  $version = Get-Version $exe $argList
  if ($version) {
    $script:rows += [pscustomobject]@{ Check = $name; Status = "PASS"; Detail = $version }
  } else {
    $status = if ($required) { "FAIL" } else { "WARN" }
    $script:rows += [pscustomobject]@{ Check = $name; Status = $status; Detail = $hint }
    if ($required) { $script:failures += $name }
  }
}

Add-Check "gcc"     "C:\msys64\ucrt64\bin\gcc.exe"     @("--version") $true  "install mingw-w64-ucrt-x86_64-toolchain"
Add-Check "clang"   "C:\msys64\ucrt64\bin\clang.exe"   @("--version") $false "optional second compiler (UCRT64 clang)"
Add-Check "cmake"   "C:\msys64\ucrt64\bin\cmake.exe"   @("--version") $true  "install mingw-w64-ucrt-x86_64-cmake"
Add-Check "ninja"   "C:\msys64\ucrt64\bin\ninja.exe"   @("--version") $true  "install mingw-w64-ucrt-x86_64-ninja"
Add-Check "ctest"   "C:\msys64\ucrt64\bin\ctest.exe"   @("--version") $true  "ships with cmake"
Add-Check "python"  "python"                            @("--version") $true  "install a standalone CPython 3.10+"
Add-Check "uv"      "uv"                                @("--version") $false "optional: uv venv for the Python environment"
Add-Check "micromamba" "micromamba"                     @("--version") $false "optional: conda-forge env for pinocchio"

$rows | Format-Table -AutoSize | Out-String -Width 200 | Write-Host

if (Test-Path "C:\msys64\ucrt64\bin\ccache.exe") {
  Write-Host "ccache: present (wire it via CMAKE_CXX_COMPILER_LAUNCHER in a preset)"
} else {
  Write-Host "ccache: missing (optional, recommended for rebuild time)"
}

$helper = (git config --get credential.helper) 2>$null
if ([string]::IsNullOrWhiteSpace($helper)) {
  Write-Host "git credential.helper: MISSING (push will fail non-interactively)"
} else {
  Write-Host "git credential.helper: $helper"
}

if ($failures.Count -gt 0) {
  Write-Host ("FAIL: missing required tools: " + ($failures -join ", "))
  exit 1
}
Write-Host "PASS: required toolchain present"
exit 0