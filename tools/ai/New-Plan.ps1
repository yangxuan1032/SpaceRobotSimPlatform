#requires -Version 5.1
<#
.SYNOPSIS
    Create a plan file inside <model>/Logs/Plan.

.DESCRIPTION
    Output: <model>/Logs/Plan/YYYY-MM-DD-<slug>.md
    Edit the same file when a plan evolves; do not create duplicates.

.EXAMPLE
    .\New-Plan.ps1 -Model deepseek -Title "SRSP architecture" -Slug "srsp-architecture" -Status planning
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Title,

    [string]$Slug = '',

    [string]$Status = 'planning',

    [string]$Model = '',

    [string]$Root = ''
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Resolve-AiModel.ps1')

$context = Resolve-SrspAiModel -Model $Model -Root $Root

$statusText = $Status
$labelsPath = Join-Path $PSScriptRoot 'labels.json'
if (Test-Path -LiteralPath $labelsPath) {
    $labelJson = [System.IO.File]::ReadAllText($labelsPath, [System.Text.Encoding]::UTF8)
    $labels = $labelJson | ConvertFrom-Json
    if ($null -ne $labels.statuses) {
        $mapped = $labels.statuses.$Status
        if ($null -ne $mapped) {
            $statusText = [string]$mapped
        }
    }
}

if (-not $Slug) {
    $Slug = 'plan'
}
$Slug = $Slug.ToLowerInvariant() -replace '[^a-z0-9]+', '-'
$Slug = $Slug.Trim('-')
if (-not $Slug) {
    $Slug = 'plan'
}

$planDirectory = $context.Plan
New-Item -ItemType Directory -Force -Path $planDirectory | Out-Null

$templatePath = Join-Path $PSScriptRoot 'templates\plan-template.md'
if (-not (Test-Path -LiteralPath $templatePath)) {
    throw "plan template not found: $templatePath"
}

$date = Get-Date -Format 'yyyy-MM-dd'
$baseName = "$date-$Slug"
$path = Join-Path $planDirectory "$baseName.md"
$index = 2
while (Test-Path -LiteralPath $path) {
    $path = Join-Path $planDirectory "$baseName-$index.md"
    $index++
}

$template = [System.IO.File]::ReadAllText($templatePath, [System.Text.Encoding]::UTF8)
$text = $template.Replace('{{TITLE}}', $Title)
$text = $text.Replace('{{DATE}}', $date)
$text = $text.Replace('{{STATUS}}', $statusText)

$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($path, $text, $utf8NoBom)

Write-Host "Created plan for '$($context.Model)': $path"
Write-Output $path
