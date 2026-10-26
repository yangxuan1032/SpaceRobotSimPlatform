# Runs a Python interop script with the pinned interpreter.
# Usage: pwsh -File tools/interop/run.ps1 compare_pinocchio.py <model>
$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here "..\..")
$python = Join-Path $root "deepseek\Temp\venv\Scripts\python.exe"
if (-not (Test-Path $python)) {
  Write-Error "Python environment missing: $python (create it with 'uv venv deepseek\Temp\venv --python 3.12')"
  exit 2
}
$script = $args[0]
$rest = @($args | Select-Object -Skip 1)
& $python (Join-Path $here $script) @rest
exit $LASTEXITCODE