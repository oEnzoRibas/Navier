@echo off
pwsh -NoProfile -File "%~dp0tools\run_navier.ps1" %*
if errorlevel 1 pause
