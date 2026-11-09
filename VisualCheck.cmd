@echo off
rem Double-click launcher for the SRSP visual check (see docs/10).
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\visual\VisualCheck.ps1" %*
if errorlevel 1 (
  echo.
  echo Visual check failed; see the output above.
  pause
)
endlocal
