[CmdletBinding()]
param(
    [ValidateSet('Showcase','Shear2D','Fluid3D','Cavity2D','Heat2D','Vortex2D')]
    [string]$Scenario = 'Showcase',
    [switch]$NoOpen
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot 'build-msvc'
$exeDir = Join-Path $buildDir 'Release'
$runId = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6)
$runDir = Join-Path (Join-Path $projectRoot 'runs') $runId
New-Item -ItemType Directory -Path $runDir -Force | Out-Null
$scenarios = if ($Scenario -eq 'Showcase') { @('Shear2D','Fluid3D') } else { @($Scenario) }
$specs = @{
    Shear2D = @{Target='navier_shear2d';File='shear2d.data';Grid=192;Steps=4000;Interval=80}
    Fluid3D = @{Target='navier_fluid3d';File='fluid3d.data';Grid=32;Steps=600;Interval=20}
    Cavity2D = @{Target='navier_cavity2d';File='cavity2d.data';Grid=64;Steps=6000;Interval=60}
    Heat2D = @{Target='navier_heat2d';File='heat2d.data';Grid=64;Steps=400;Interval=20}
    Vortex2D = @{Target='navier_fluid2d';File='fluid2d.data';Grid=64;Steps=400;Interval=20}
}
$targets = @('navier_viewer') + @($scenarios | ForEach-Object {$specs[$_].Target})
if (-not (Test-Path -LiteralPath (Join-Path $buildDir 'CMakeCache.txt'))) {
    & cmake -S $projectRoot -B $buildDir -G 'Visual Studio 18 2026' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
}
& cmake --build $buildDir --config Release --parallel 1 --target @targets
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
$datasets = @()
foreach ($name in $scenarios) {
    $spec = $specs[$name]
    $output = Join-Path $runDir $spec.File
    Write-Host "Running $name -> $output"
    & (Join-Path $exeDir ($spec.Target + '.exe')) $output $spec.Grid $spec.Steps $spec.Interval
    if ($LASTEXITCODE -ne 0) { throw "Simulation $name failed" }
    $datasets += $output
}
$viewerPath = Join-Path $runDir 'navier-viewer.html'
& (Join-Path $exeDir 'navier_viewer.exe') $viewerPath @datasets
if ($LASTEXITCODE -ne 0) { throw 'Viewer export failed' }
Write-Host "Ready: $viewerPath"
if (-not $NoOpen) { Invoke-Item -LiteralPath $viewerPath }
