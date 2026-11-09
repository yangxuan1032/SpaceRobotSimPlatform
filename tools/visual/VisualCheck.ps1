#requires -Version 5.1
<#
.SYNOPSIS
    Renders the visual-check scene and opens the snapshots for human inspection.

.DESCRIPTION
    Builds srsp-studio for the selected model namespace, renders the reference
    scene with the Qt and RHI backends, writes PNGs under <model>/Temp/visual/
    and opens them unless -NoOpen is given. The Qt snapshot is required; the RHI
    snapshot is best effort (a machine without D3D11 still gets the Qt image).

.EXAMPLE
    .\tools\visual\VisualCheck.ps1

.EXAMPLE
    .\tools\visual\VisualCheck.ps1 -Renderer qt -NoOpen
#>
[CmdletBinding()]
param(
    [string]$Model = '',
    [string]$Preset = 'win-ucrt64-debug',
    [ValidateSet('qt', 'rhi', 'both')]
    [string]$Renderer = 'both',
    [switch]$NoOpen
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Set-Location $root

if (-not $Model) { $Model = $env:SRSP_AI_MODEL }
if (-not $Model) { $Model = 'deepseek' }

Write-Host "== SRSP visual check ==" -ForegroundColor Cyan
Write-Host "root   : $root"
Write-Host "model  : $Model"
Write-Host "preset : $Preset"

# A double-clicked shell does not always inherit the MSYS2 toolchain.
$msys = 'C:\msys64\ucrt64\bin'
if ((Test-Path $msys) -and ($env:Path -notlike "*$msys*")) {
    $env:Path = "$msys;$env:Path"
}

. (Join-Path $root 'tools\ai\Enter-AgentEnv.ps1') -Model $Model

$buildDir = Join-Path $root "$Model/Temp/build/$Preset"
if (-not (Test-Path (Join-Path $buildDir 'CMakeCache.txt'))) {
    Write-Host "configuring (first run)..." -ForegroundColor Yellow
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw "cmake --preset $Preset failed" }
}

Write-Host "building srsp-studio..." -ForegroundColor Yellow
& cmake --build --preset $Preset --target srsp_studio
if ($LASTEXITCODE -ne 0) { throw "build failed" }

$exe = Join-Path $buildDir 'bin/srsp-studio.exe'
if (-not (Test-Path $exe)) { throw "studio binary not found: $exe" }

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$outDir = Join-Path $root "$Model/Temp/visual/$stamp"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$renderers = switch ($Renderer) {
    'both' { @('qt', 'rhi') }
    default { @($Renderer) }
}

$results = @()
$qtRendered = $false
$failures = @()
foreach ($backend in $renderers) {
    $png = Join-Path $outDir "$backend.png"
    Write-Host "rendering $backend ..." -ForegroundColor Yellow
    & $exe --snapshot $png --renderer $backend
    $ok = ($LASTEXITCODE -eq 0) -and (Test-Path $png)
    $size = if ($ok) { (Get-Item $png).Length } else { 0 }
    $results += [pscustomobject]@{ Backend = $backend; Ok = $ok; Bytes = $size; Path = $png }
    if ($ok -and $backend -eq 'qt') { $qtRendered = $true }
    if (-not $ok) { $failures += $backend }
    if (-not $ok) {
        Write-Warning "$backend snapshot failed (backend may be unavailable on this machine)"
    }
}

Write-Host ''
Write-Host '== snapshots ==' -ForegroundColor Cyan
$results | Format-Table -AutoSize | Out-String | Write-Host

$qtRequested = $renderers -contains 'qt'
if ($qtRequested -and -not $qtRendered) { throw 'the Qt snapshot was requested but did not render' }

Write-Host 'look for: earth + terminator, yellow satellite, green rotated box,'
Write-Host '         and on the RHI image the "SRSP VISUAL CHECK" text at the top left.'
Write-Host "images: $outDir"

if (-not $NoOpen) {
    foreach ($result in $results) {
        if ($result.Ok) { Start-Process $result.Path }
    }
}

if ($failures.Count -gt 0) { exit 2 }
exit 0
