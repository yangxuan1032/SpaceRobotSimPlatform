#requires -Version 5.1
<#
.SYNOPSIS
    Scaffold a model namespace: <model>/{Temp,Logs/Plan,Logs/Log}.

.DESCRIPTION
    Idempotent. Creates the directory tree, a model README and (when missing)
    a change-log header. Existing files are kept unless -Force is used.

.EXAMPLE
    .\New-AiModelWorkspace.ps1 -Model claude
#>
[CmdletBinding()]
param(
    [string]$Model = '',
    [string]$Root = '',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Resolve-AiModel.ps1')

$context = Resolve-SrspAiModel -Model $Model -Root $Root

$directories = @(
    $context.Home,
    $context.Temp,
    (Join-Path $context.Temp 'tmp'),
    (Join-Path $context.Temp 'build'),
    (Join-Path $context.Temp 'cache'),
    $context.Plan,
    $context.Log
)

foreach ($directory in $directories) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
$date = Get-Date -Format 'yyyy-MM-dd'

$readmePath = Join-Path $context.Home 'README.md'
if ($Force -or -not (Test-Path -LiteralPath $readmePath)) {
    $template = [System.IO.File]::ReadAllText(
        (Join-Path $PSScriptRoot 'templates\model-README.md'),
        [System.Text.Encoding]::UTF8)
    $text = $template.Replace('{{MODEL}}', $context.Model)
    $text = $text.Replace('{{DATE}}', $date)
    [System.IO.File]::WriteAllText($readmePath, $text, $utf8NoBom)
}

$logPath = Join-Path $context.Log 'CHANGELOG.md'
if (-not (Test-Path -LiteralPath $logPath)) {
    $header = [System.IO.File]::ReadAllText(
        (Join-Path $PSScriptRoot 'templates\changelog-header.md'),
        [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($logPath, $header, $utf8NoBom)
}

Write-Host "Model workspace ready: $($context.Home)"
Write-Host "  cache: $($context.Temp)"
Write-Host "  plans: $($context.Plan)"
Write-Host "  log  : $logPath"
