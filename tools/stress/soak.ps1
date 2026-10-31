# Soak test driver: runs srsp-cli soak and samples process counters.
#
# Usage:
#   pwsh -File tools/stress/soak.ps1 -Minutes 8 -Plugin <hello_plugin.dll>
#        [-IntervalSec 10] [-OutCsv <path>] [-Cli <path>]
#        [-MaxRssGrowthPct 2] [-MaxHandleGrowth 5]
#
# Exit codes: 0 pass, 1 threshold exceeded, 2 startup/crash failure.

[CmdletBinding()]
param(
  [int]$Minutes = 8,
  [string]$Plugin = "",
  [string]$Cli = "",
  [int]$IntervalSec = 10,
  [double]$MaxRssGrowthPct = 2.0,
  [int]$MaxHandleGrowth = 5,
  [string]$Workload = "ping",
  [string]$Model = "",
  [string]$OutCsv = ""
)

$ErrorActionPreference = "Stop"
trap {
  Write-Host ("TRAP line {0}: {1}" -f $_.InvocationInfo.ScriptLineNumber, $_.Exception.Message)
  exit 2
}
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here "..\..")

if ([string]::IsNullOrWhiteSpace($Cli)) {
  $Cli = Join-Path $root "deepseek\Temp\build\win-ucrt64-debug\bin\srsp-cli.exe"
}
if ([string]::IsNullOrWhiteSpace($Plugin)) {
  $Plugin = Join-Path $root "deepseek\Temp\build\win-ucrt64-debug\plugins\org.srsp.examples.hello\bin\srsp_hello.dll"
}
if ([string]::IsNullOrWhiteSpace($OutCsv)) {
  $stamp = Get-Date -Format "yyyyMMdd-HHmmss"
  $OutCsv = Join-Path $root "deepseek\Temp\soak-$stamp.csv"
}

foreach ($path in @($Cli, $Plugin)) {
  if (-not (Test-Path $path)) {
    Write-Error "missing required file: $path"
    exit 2
  }
}

$stdout = [System.IO.Path]::ChangeExtension($OutCsv, ".out.txt")
$stderr = [System.IO.Path]::ChangeExtension($OutCsv, ".err.txt")
$start = Get-Date

Write-Host "soak: minutes=$Minutes interval=${IntervalSec}s"
Write-Host "  cli:    $Cli"
Write-Host "  plugin: $Plugin"
Write-Host "  csv:    $OutCsv"

$cliArgs = @("soak", "--plugin", $Plugin, "--minutes", "$Minutes", "--sleep-ms", "5", "--workload", $Workload)
if (-not [string]::IsNullOrWhiteSpace($Model)) { $cliArgs += @("--model", $Model) }
Write-Host "  workload: $Workload"
# The CLI is a MinGW binary: it needs the UCRT64 runtime DLLs on PATH even when
# this script is launched from a shell that never sourced the MSYS2 environment.
$mingwBin = "C:\msys64\ucrt64\bin"
if (Test-Path $mingwBin) {
  $env:PATH = "$mingwBin;$env:PATH"
}

$process = Start-Process -FilePath $Cli `
  -ArgumentList $cliArgs -WorkingDirectory $root `
  -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru -WindowStyle Hidden

$rows = New-Object System.Collections.Generic.List[object]
$samples = 0
while (-not $process.HasExited) {
  Start-Sleep -Seconds $IntervalSec
  $proc = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
  if ($null -eq $proc) { break }
  $elapsed = [math]::Round(((Get-Date) - $start).TotalSeconds, 1)
  $row = [pscustomobject]@{
    timestamp = (Get-Date).ToString("o")
    elapsed_s = $elapsed
    rss_mb    = [math]::Round($proc.WorkingSet64 / 1MB, 2)
    private_mb= [math]::Round($proc.PrivateMemorySize64 / 1MB, 2)
    handles   = $proc.HandleCount
    cpu_s     = [math]::Round($proc.TotalProcessorTime.TotalSeconds, 2)
  }
  $rows.Add($row)
  $samples++
  Write-Host ("  t={0,6:N1}s rss={1,8:N2}MB handles={2,5}" -f $row.elapsed_s, $row.rss_mb, $row.handles)
}

$process.WaitForExit()
$exitCode = $process.ExitCode
$summaryLineEarly = (Get-Content $stdout -ErrorAction SilentlyContinue | Select-String "SUMMARY" | Select-Object -Last 1)
$earlyFailures = 0
if ($summaryLineEarly -and $summaryLineEarly.Line -match "failures=(\d+)") { $earlyFailures = [int]$matches[1] }
if ($null -eq $exitCode) { $exitCode = if ($earlyFailures -eq 0) { 0 } else { 1 } }
$rows | Export-Csv -Path $OutCsv -NoTypeInformation -Encoding UTF8

# Baseline = first sample after the 60 s warm-up; final = mean of the last three.
$warm = $rows | Where-Object { $_.elapsed_s -ge 60 }
if ($null -eq $warm -or @($warm).Count -eq 0) { $warm = $rows }
$baseline = $warm | Select-Object -First 1
if ($null -eq $baseline) { $baseline = $rows | Select-Object -First 1 }
$last = @($rows | Select-Object -Last 3)
$finalRss = ($last | Measure-Object -Property rss_mb -Average).Average
$finalHandles = ($last | Measure-Object -Property handles -Average).Average
$rssGrowth = if ($baseline.rss_mb -gt 0) { (($finalRss - $baseline.rss_mb) / $baseline.rss_mb) * 100.0 } else { 0.0 }
$handleGrowth = [math]::Round($finalHandles - $baseline.handles, 0)

$summaryLine = (Get-Content $stdout -ErrorAction SilentlyContinue | Select-String "SUMMARY" | Select-Object -Last 1)
$failures = 0
if ($summaryLine -and $summaryLine.Line -match "failures=(\d+)") { $failures = [int]$matches[1] }

$verdict = "PASS"
$reasons = @()
if (-not $summaryLine) { $verdict = "FAIL"; $reasons += "no-summary" }
if ($samples -eq 0) { $verdict = "FAIL"; $reasons += "no-samples" }
if ($exitCode -ne 0) { $verdict = "FAIL"; $reasons += "exit=$exitCode" }
if ($failures -ne 0) { $verdict = "FAIL"; $reasons += "failures=$failures" }
if ($rssGrowth -gt $MaxRssGrowthPct) { $verdict = "FAIL"; $reasons += ("rss+{0:N2}%" -f $rssGrowth) }
if ($handleGrowth -gt $MaxHandleGrowth) { $verdict = "FAIL"; $reasons += "handles+$handleGrowth" }

Write-Host ""
Write-Host ("SOAK_RESULT {0} samples={1} rss_base={2}MB rss_final={3}MB growth={4:N2}% handle_growth={5} failures={6} exit={7} {8}" -f `
  $verdict, $samples, $baseline.rss_mb, [math]::Round($finalRss, 2), $rssGrowth, $handleGrowth, $failures, $exitCode, ($reasons -join " "))
Write-Host "  csv: $OutCsv"

if ($verdict -eq "PASS") { exit 0 } else { exit 1 }
