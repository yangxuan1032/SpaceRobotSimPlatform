#requires -Version 5.1
<#
.SYNOPSIS
    Launch VS Code with the MSYS2 UCRT64 toolchain and a model namespace.

.DESCRIPTION
    CMake Tools, clangd and the debugger run inside the VS Code extension host,
    so both the MSYS2 directories and SRSP_AI_MODEL must be inherited from the
    process that starts VS Code. Launch this script instead of opening the
    folder directly.

    The model namespace selects the cache root (<model>/Temp) used by the
    CMake presets and clangd. Resolution order: -Model, $env:SRSP_AI_MODEL,
    $env:AI_MODEL.

.PARAMETER Model
    Model slug, for example deepseek or claude. See ai-models.json.

.PARAMETER Path
    Repository root to open. Defaults to two levels above this script.

.PARAMETER Msys2Root
    MSYS2 installation root. Defaults to $env:MSYS2_ROOT or C:\msys64.
#>
param(
    [string]$Model = '',
    [string]$Path = '',
    [string]$Msys2Root = ''
)

$ErrorActionPreference = 'Stop'

if (-not $Msys2Root) {
    if ($env:MSYS2_ROOT) { $Msys2Root = $env:MSYS2_ROOT } else { $Msys2Root = 'C:\msys64' }
}

$ucrtBin = Join-Path $Msys2Root 'ucrt64\bin'
if (-not (Test-Path -LiteralPath $ucrtBin)) {
    throw "MSYS2 UCRT64 toolchain not found at '$ucrtBin'. Install MSYS2 and run tools/env/bootstrap-msys2.sh first."
}

if (-not $Path) {
    $Path = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
}

. (Join-Path $PSScriptRoot '..\ai\Resolve-AiModel.ps1')
$context = Resolve-SrspAiModel -Model $Model -Root $Path

$tmp = Join-Path $context.Temp 'tmp'
$cache = Join-Path $context.Temp 'cache'
$directories = @(
    $tmp,
    (Join-Path $context.Temp 'build'),
    $cache,
    (Join-Path $cache 'ccache'),
    (Join-Path $cache 'pip'),
    (Join-Path $cache 'pycache'),
    $context.Plan,
    $context.Log
)
foreach ($directory in $directories) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$env:PATH = "$ucrtBin;$env:PATH"
$env:MSYSTEM = 'UCRT64'
$env:CHERE_INVOKING = '1'
$env:SRSP_PROJECT_ROOT = $context.Root
$env:SRSP_AI_MODEL = $context.Model
$env:SRSP_AI_HOME = $context.Home
$env:TEMP = $tmp
$env:TMP = $tmp
$env:TMPDIR = $tmp
$env:CCACHE_DIR = Join-Path $cache 'ccache'
$env:PIP_CACHE_DIR = Join-Path $cache 'pip'
$env:PYTHONPYCACHEPREFIX = Join-Path $cache 'pycache'

$code = Get-Command code -ErrorAction SilentlyContinue
if (-not $code) {
    throw "VS Code command 'code' not found on PATH. In VS Code run 'Shell Command: Install code command in PATH'."
}

Write-Host "MSYS2 UCRT64 : $ucrtBin"
Write-Host "AI model     : $($context.Model)"
Write-Host "Model home   : $($context.Home)"
Write-Host "Workspace    : $Path"
& $code.Source $Path
