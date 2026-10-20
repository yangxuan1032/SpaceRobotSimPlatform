#requires -Version 5.1
<#
.SYNOPSIS
    Shared helpers for the per-model agent workspace.

.DESCRIPTION
    Dot-source this file, then call Resolve-SrspAiModel. It resolves:
      1. the -Model argument,
      2. $env:SRSP_AI_MODEL,
      3. $env:AI_MODEL,
      4. otherwise throws.

    It maps aliases from ai-models.json to the canonical slug, validates the
    name, and returns the per-model paths:

      <root>/<slug>/Temp
      <root>/<slug>/Logs/Plan
      <root>/<slug>/Logs/Log
#>

function Get-SrspProjectRoot {
    param([string]$Root = '')

    if ($Root) {
        return (Resolve-Path -LiteralPath $Root).Path
    }
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Get-SrspAiRegistry {
    param([string]$Root)

    $path = Join-Path $Root 'ai-models.json'
    if (-not (Test-Path -LiteralPath $path)) {
        return $null
    }
    $json = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    return $json | ConvertFrom-Json
}

function Resolve-SrspAiModel {
    param(
        [string]$Model = '',
        [string]$Root = ''
    )

    $rootPath = Get-SrspProjectRoot -Root $Root

    $candidate = $Model
    if (-not $candidate) { $candidate = $env:SRSP_AI_MODEL }
    if (-not $candidate) { $candidate = $env:AI_MODEL }
    if (-not $candidate) {
        throw "AI model namespace is not set. Pass -Model <slug> or set SRSP_AI_MODEL. Known models: deepseek, claude, gpt, gemini, qwen, glm, llama, mistral, grok, kimi, doubao, ernie, agent."
    }

    $slug = $candidate.ToLowerInvariant().Trim()
    $slug = $slug -replace '[^a-z0-9]+', '-'
    $slug = $slug.Trim('-')
    if (-not $slug) {
        throw "Invalid model name: '$candidate'."
    }

    $registry = Get-SrspAiRegistry -Root $rootPath
    if ($null -ne $registry -and $null -ne $registry.models) {
        $aliasMap = @{}
        foreach ($entry in $registry.models) {
            $canonical = [string]$entry.slug
            $aliasMap[$canonical.ToLowerInvariant()] = $canonical
            foreach ($alias in @($entry.aliases)) {
                if ($null -ne $alias) {
                    $aliasMap[([string]$alias).ToLowerInvariant()] = $canonical
                }
            }
        }
        if ($aliasMap.ContainsKey($slug)) {
            $slug = $aliasMap[$slug]
        }
    }

    if ($slug -notmatch '^[a-z0-9][a-z0-9-]{0,31}$') {
        throw "Invalid model namespace '$slug'. Use 1-32 lowercase letters, digits or hyphens."
    }

    $reserved = @(
        'tools', 'docs', 'api', 'examples', 'src', 'core', 'plugins',
        'apps', 'sdk', 'tests', 'third-party', 'third_party', 'build',
        'dist', 'temp', 'logs'
    )
    if ($reserved -contains $slug) {
        throw "Model namespace '$slug' is reserved by the project layout."
    }

    $modelHome = Join-Path $rootPath $slug
    $logs = Join-Path $modelHome 'Logs'

    return [pscustomobject]@{
        Model = $slug
        Root = $rootPath
        Home = $modelHome
        Temp = (Join-Path $modelHome 'Temp')
        Plan = (Join-Path $logs 'Plan')
        Log = (Join-Path $logs 'Log')
    }
}

function Get-SrspLabel {
    param(
        [string]$Name,
        [string]$Fallback = ''
    )

    $path = Join-Path $PSScriptRoot 'labels.json'
    if (-not (Test-Path -LiteralPath $path)) {
        return $Fallback
    }
    $json = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8)
    $labels = $json | ConvertFrom-Json
    $value = $labels.$Name
    if ($null -eq $value) {
        return $Fallback
    }
    return [string]$value
}
