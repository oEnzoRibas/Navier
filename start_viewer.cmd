@echo off
setlocal
cd /d "%~dp0viewer"
where npm >nul 2>nul
if errorlevel 1 (
  echo Instale Node.js 20.19+ ou 22.12+ antes de iniciar.
  exit /b 1
)
if not exist node_modules (
  call npm ci
  if errorlevel 1 exit /b 1
)
call npm run build
if errorlevel 1 exit /b 1
echo Navier Viewer: http://127.0.0.1:4173 - encerre com Ctrl+C
call npm run preview
