#requires -Version 5.1
<#
.SYNOPSIS
    Append one entry to <model>/Logs/Log/CHANGELOG.md.

.DESCRIPTION
    - entries are grouped by date and store the date only;
    - the next Vx number is computed within the current day;
    - directories and the log file are created when missing.

    Chinese labels come from tools/ai/labels.json so this script stays pure
    ASCII and keeps working with Windows PowerShell 5.1.

.EXAMPLE
    .\Append-ChangeLog.ps1 -Model deepseek -Title "Move build cache" -Type config `
        -Files "CMakePresets.json","tools/ai/Enter-AgentEnv.ps1" `
        -Summary "Build tree follows the active model namespace" `
        -Verification "cmake --preset win-ucrt64-debug"
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Title,

    [Parameter(Mandatory = $true)]
    [string[]]$Files,

    [Parameter(Mandatory = $true)]
    [string]$Summary,

    [string]$Type = 'modify',

    [string]$Verification = '',

    [string]$Plan = '',

    [string]$Model = '',

    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Resolve-AiModel.ps1')

$context = Resolve-SrspAiModel -Model $Model -Root $Root

$labels = $null
$labelPath = Join-Path $PSScriptRoot 'labels.json'
if (Test-Path -LiteralPath $labelPath) {
    $labelJson = [System.IO.File]::ReadAllText($labelPath, [System.Text.Encoding]::UTF8)
    $labels = $labelJson | ConvertFrom-Json
}

function Get-Label([string]$Name, [string]$Fallback) {
    if ($null -ne $labels) {
        $value = $labels.$Name
        if ($null -ne $value) {
            return [string]$value
        }
    }
    return $Fallback
}

$typeText = $Type
if ($null -ne $labels -and $null -ne $labels.types) {
    $mapped = $labels.types.$Type
    if ($null -ne $mapped) {
        $typeText = [string]$mapped
    }
}

$colon = Get-Label 'colon' ': '
$dash = Get-Label 'dash' '-'
$labelType = Get-Label 'type' 'Type'
$labelFiles = Get-Label 'files' 'Files'
$labelSummary = Get-Label 'summary' 'Summary'
$labelVerification = Get-Label 'verification' 'Verification'
$labelPlan = Get-Label 'plan' 'Plan'

$logPath = Join-Path $context.Log 'CHANGELOG.md'
New-Item -ItemType Directory -Force -Path $context.Log | Out-Null

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)

if (-not (Test-Path -LiteralPath $logPath)) {
    $headerPath = Join-Path $PSScriptRoot 'templates\changelog-header.md'
    $header = [System.IO.File]::ReadAllText($headerPath, [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($logPath, $header, $utf8NoBom)
}

$content = [System.IO.File]::ReadAllText($logPath, [System.Text.Encoding]::UTF8)
$content = $content -replace "`r`n", "`n"

$lines = [System.Collections.Generic.List[string]]::new()
foreach ($line in ($content -split "`n")) {
    $lines.Add($line)
}

$date = Get-Date -Format 'yyyy-MM-dd'
$dateHeading = "## $date"

$start = -1
for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i].TrimEnd() -eq $dateHeading) {
        $start = $i
        break
    }
}

if ($start -ge 0) {
    $end = $lines.Count
    for ($i = $start + 1; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^## ') {
            $end = $i
            break
        }
    }

    $maxVersion = 0
    for ($i = $start + 1; $i -lt $end; $i++) {
        if ($lines[$i] -match '^### V(\d+)') {
            $candidate = [int]$Matches[1]
            if ($candidate -gt $maxVersion) {
                $maxVersion = $candidate
            }
        }
    }
    $nextVersion = $maxVersion + 1
}
else {
    $end = $lines.Count
    $nextVersion = 1
}

$entry = [System.Collections.Generic.List[string]]::new()
$entry.Add("### V$nextVersion $dash $Title")
$entry.Add('')
$entry.Add("- $labelType$colon$typeText")
$entry.Add("- $labelFiles$colon")
foreach ($file in $Files) {
    $entry.Add("  - ``$file``")
}
$entry.Add("- $labelSummary$colon$Summary")
if ($Verification) {
    $entry.Add("- $labelVerification$colon$Verification")
}
if ($Plan) {
    $entry.Add("- $labelPlan$colon``$Plan``")
}
$entry.Add('')

$before = [System.Collections.Generic.List[string]]::new()
for ($i = 0; $i -lt $end; $i++) {
    $before.Add($lines[$i])
}
while ($before.Count -gt 0 -and $before[$before.Count - 1] -eq '') {
    $before.RemoveAt($before.Count - 1)
}

$after = [System.Collections.Generic.List[string]]::new()
for ($i = $end; $i -lt $lines.Count; $i++) {
    $after.Add($lines[$i])
}
while ($after.Count -gt 0 -and $after[$after.Count - 1] -eq '') {
    $after.RemoveAt($after.Count - 1)
}

$result = [System.Collections.Generic.List[string]]::new()
foreach ($line in $before) {
    $result.Add($line)
}
if ($result.Count -gt 0) {
    $result.Add('')
}
if ($start -lt 0) {
    $result.Add($dateHeading)
    $result.Add('')
}
foreach ($line in $entry) {
    $result.Add($line)
}
if ($after.Count -gt 0) {
    $result.Add('')
    foreach ($line in $after) {
        $result.Add($line)
    }
}

$text = ($result -join "`n").TrimEnd() + "`n"
[System.IO.File]::WriteAllText($logPath, $text, $utf8NoBom)

Write-Host "Appended V$nextVersion for '$($context.Model)' - $Title"
Write-Host "Log: $logPath"
