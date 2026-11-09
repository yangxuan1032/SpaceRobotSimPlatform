#requires -Version 5.1
<#
.SYNOPSIS
    Redirect temporary directories and tool caches into <model>/Temp.

.DESCRIPTION
    Dot-source it so the variables stay in the current scope:

        Set-ExecutionPolicy -Scope Process Bypass -Force
        . .\tools\ai\Enter-AgentEnv.ps1 -Model deepseek

    Model resolution order: -Model, $env:SRSP_AI_MODEL, $env:AI_MODEL.
    Only the current process and its children are affected.

.NOTES
    Dot-sourcing binds this script's parameters into the caller scope. To keep
    that surface narrow the parameters are -Model and -ProjectRoot (alias
    -Root), and every internal variable is prefixed with $srsp so common caller
    names such as $root, $tmp or $cache are left alone.
#>
[CmdletBinding()]
param(
    [string]$Model = '',
    [Alias('Root')]
    [string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Resolve-AiModel.ps1')

$srspContext = Resolve-SrspAiModel -Model $Model -Root $ProjectRoot

$srspTmp = Join-Path $srspContext.Temp 'tmp'
$srspCache = Join-Path $srspContext.Temp 'cache'

$srspDirectories = @(
    $srspContext.Home,
    $srspContext.Temp,
    $srspTmp,
    (Join-Path $srspContext.Temp 'build'),
    $srspCache,
    (Join-Path $srspCache 'ccache'),
    (Join-Path $srspCache 'pip'),
    (Join-Path $srspCache 'npm'),
    (Join-Path $srspCache 'pycache'),
    (Join-Path $srspCache 'xdg'),
    $srspContext.Plan,
    $srspContext.Log
)

foreach ($srspDirectory in $srspDirectories) {
    New-Item -ItemType Directory -Force -Path $srspDirectory | Out-Null
}

$env:SRSP_PROJECT_ROOT = $srspContext.Root
$env:SRSP_AI_MODEL = $srspContext.Model
$env:SRSP_AI_HOME = $srspContext.Home

$env:TEMP = $srspTmp
$env:TMP = $srspTmp
$env:TMPDIR = $srspTmp
$env:XDG_CACHE_HOME = Join-Path $srspCache 'xdg'
$env:PIP_CACHE_DIR = Join-Path $srspCache 'pip'
$env:CCACHE_DIR = Join-Path $srspCache 'ccache'
$env:NPM_CONFIG_CACHE = Join-Path $srspCache 'npm'
$env:PYTHONPYCACHEPREFIX = Join-Path $srspCache 'pycache'

# Test fixtures: a fresh shell should be able to run the suite without manual
# exports. Values are only set when the corresponding artifact exists.
$srspModels = Join-Path $srspContext.Root 'examples/models'
if (Test-Path $srspModels) {
    $env:SRSP_TEST_MODELS = $srspModels
}

$srspPlugin = $null
foreach ($srspPreset in @('win-ucrt64-debug', 'win-ucrt64-release')) {
    foreach ($srspRelative in @(
        "build/$srspPreset/plugins/org.srsp.examples.hello/bin/srsp_hello.dll",
        "build/$srspPreset/bin/srsp_hello.dll")) {
        $srspCandidate = Join-Path $srspContext.Temp $srspRelative
        if ((-not $srspPlugin) -and (Test-Path $srspCandidate)) {
            $srspPlugin = $srspCandidate
        }
    }
}
if ($srspPlugin) {
    $env:SRSP_TEST_PLUGIN = $srspPlugin
} else {
    Remove-Item Env:SRSP_TEST_PLUGIN -ErrorAction SilentlyContinue
}

Write-Host "AI model namespace : $($srspContext.Model)"
Write-Host "Model home        : $($srspContext.Home)"
Write-Host "TEMP/TMP/TMPDIR   : $srspTmp"
Write-Host "CCACHE_DIR        : $env:CCACHE_DIR"
Write-Host "PIP_CACHE_DIR     : $env:PIP_CACHE_DIR"
Write-Host "PYTHONPYCACHEPREFIX: $env:PYTHONPYCACHEPREFIX"
if (Test-Path Env:SRSP_TEST_MODELS) {
    Write-Host "SRSP_TEST_MODELS  : $env:SRSP_TEST_MODELS"
}
if ($srspPlugin) {
    Write-Host "SRSP_TEST_PLUGIN  : $srspPlugin"
} else {
    Write-Host "SRSP_TEST_PLUGIN  : (not built yet; plugin tests will skip)"
}
