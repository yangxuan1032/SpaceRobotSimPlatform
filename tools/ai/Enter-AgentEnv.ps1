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
#>
[CmdletBinding()]
param(
    [string]$Model = '',
    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Resolve-AiModel.ps1')

$context = Resolve-SrspAiModel -Model $Model -Root $Root

$tmp = Join-Path $context.Temp 'tmp'
$cache = Join-Path $context.Temp 'cache'

$directories = @(
    $context.Home,
    $context.Temp,
    $tmp,
    (Join-Path $context.Temp 'build'),
    $cache,
    (Join-Path $cache 'ccache'),
    (Join-Path $cache 'pip'),
    (Join-Path $cache 'npm'),
    (Join-Path $cache 'pycache'),
    (Join-Path $cache 'xdg'),
    $context.Plan,
    $context.Log
)

foreach ($directory in $directories) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$env:SRSP_PROJECT_ROOT = $context.Root
$env:SRSP_AI_MODEL = $context.Model
$env:SRSP_AI_HOME = $context.Home

$env:TEMP = $tmp
$env:TMP = $tmp
$env:TMPDIR = $tmp
$env:XDG_CACHE_HOME = Join-Path $cache 'xdg'
$env:PIP_CACHE_DIR = Join-Path $cache 'pip'
$env:CCACHE_DIR = Join-Path $cache 'ccache'
$env:NPM_CONFIG_CACHE = Join-Path $cache 'npm'
$env:PYTHONPYCACHEPREFIX = Join-Path $cache 'pycache'

Write-Host "AI model namespace : $($context.Model)"
Write-Host "Model home        : $($context.Home)"
Write-Host "TEMP/TMP/TMPDIR   : $tmp"
Write-Host "CCACHE_DIR        : $env:CCACHE_DIR"
Write-Host "PIP_CACHE_DIR     : $env:PIP_CACHE_DIR"
Write-Host "PYTHONPYCACHEPREFIX: $env:PYTHONPYCACHEPREFIX"
