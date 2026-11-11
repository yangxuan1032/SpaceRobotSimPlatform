#requires -Version 5.1
<#
.SYNOPSIS
    Release gate: tests, documentation and release-only performance thresholds.

.DESCRIPTION
    Runs the checks that must pass before a release or a push that claims
    "done". Test/doc/format gates always run; performance gates need the
    release preset, because a debug build measures unoptimised code.

    Thresholds (docs/06-packaging.md):
      - ctest all green
      - docs_check and format_check green
      - bench_dynamics (release) semi <= 300 us
      - bench_world (release) step <= 1000 us
#>
[CmdletBinding()]
param(
    [string]$Model = '',
    [string]$Preset = 'win-ucrt64-debug',
    [string]$BenchPreset = 'win-ucrt64-release'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

if (-not $Model) { $Model = $env:SRSP_AI_MODEL }
if (-not $Model) { $Model = 'deepseek' }

$msys = 'C:\msys64\ucrt64\bin'
if ((Test-Path $msys) -and ($env:Path -notlike "*$msys*")) {
    $env:Path = "$msys;$env:Path"
}

. (Join-Path $root 'tools\ai\Enter-AgentEnv.ps1') -Model $Model

$failures = New-Object System.Collections.Generic.List[string]
function Gate([string]$name, [scriptblock]$check) {
    Write-Host "== $name" -ForegroundColor Cyan
    if (-not (& $check)) {
        $failures.Add($name)
        Write-Host "FAILED: $name" -ForegroundColor Red
    }
}

Gate 'ctest' {
    & ctest --preset $Preset
    return $LASTEXITCODE -eq 0
}

Gate 'docs_check' {
    & ctest --preset $Preset -R docs_check
    return $LASTEXITCODE -eq 0
}

Gate 'format_check' {
    & ctest --preset $Preset -R format_check
    return $LASTEXITCODE -eq 0
}

$benchDir = Join-Path $root "$Model/Temp/build/$BenchPreset/bin"
$benchDynamics = Join-Path $benchDir 'bench_dynamics.exe'
$benchWorld = Join-Path $benchDir 'bench_world.exe'
if ((Test-Path $benchDynamics) -and (Test-Path $benchWorld)) {
    Gate 'bench_dynamics (semi <= 300 us)' {
        $line = & $benchDynamics 'semi' 2>&1 | Select-String 'budget_us'
        Write-Host $line
        return $line -and ($line.Line -match 'verdict=PASS')
    }
    Gate 'bench_world (step <= 1000 us)' {
        $line = & $benchWorld 2>&1 | Select-String 'step_us'
        Write-Host $line
        return $line -and ($line.Line -match 'verdict=PASS')
    }
} else {
    Write-Host 'performance gates SKIPPED: no release build.' -ForegroundColor Yellow
    Write-Host "  configure: cmake --preset $BenchPreset; cmake --build --preset $BenchPreset"
}

Write-Host ''
if ($failures.Count -gt 0) {
    Write-Host "release gate FAILED: $($failures -join ', ')" -ForegroundColor Red
    exit 1
}
Write-Host 'release gate PASSED' -ForegroundColor Green
exit 0
